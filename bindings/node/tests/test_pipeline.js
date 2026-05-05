#!/usr/bin/env node
'use strict';
// Parity test: full 11-stage parse pipeline on combined samples.
// These exercises multiple stages interacting at once.
const path = require('path');
const { runTests } = require('./helpers');
const { newProto, nativeProto } = require('./native_token_patch');

// This should loop through each test and fast fail on the first mismatch.

const tests = [
`{{Short description|German philosopher (1788–1860)}}
{{Redirect|Schopenhauer}}
{{Use dmy dates|date=October 2020}}
{{Infobox philosopher
|region       = [[Western philosophy]]
|era          = [[19th-century philosophy]]
|image        = File:Arthur Schopenhauer by J Schäfer, 1859b.jpg
|caption      = Schopenhauer in 1859
|signature    = Arthur Schopenhauer Signature.svg
|name         = Arthur Schopenhauer
|birth_date   = {{birth date|1788|2|22|df=y}}
|birth_place  = [[Danzig]] (Gdańsk),<!--keep both names as per the Danzig/Gdansk vote (Talk:Gdańsk/Vote): "the first occurrence of the name should be used in the form Danzig (Gdansk)"--> [[Crown of the Kingdom of Poland]], [[Polish–Lithuanian Commonwealth]]
|death_date   = {{Death date and age|1860|9|21|1788|2|22|df=y}}
|death_place  = [[Free City of Frankfurt|Frankfurt]], [[German Confederation]]
|education    = {{plainlist|
* [[Ernestine Gymnasium, Gotha|Illustrious Gymnasium]]
* [[Georg August University of Göttingen|University of Göttingen]]
* [[University of Jena]] (PhD, 1813)}}
|academic_advisors = [[Gottlob Ernst Schulze]]<br>[[Karl Christian Friedrich Krause]]
| children          = 2{{efn|Despite his anti-natalist views and never marrying, Schopenhauer had at least two illegitimate daughters (in 1819 and 1836), both of whom died in infancy.}} {{r|Cartwright-2010|p=25}}
|relatives    = {{Plainlist|
* [[Johanna Schopenhauer]] (mother)
* [[Adele Schopenhauer]] (sister)
}}
|thesis_title = On the Fourfold Root of the Principle of Sufficient Reason
|thesis_year=1813| thesis_url=https://www.gutenberg.org/cache/epub/50966/pg50966-images.html
|institutions = [[Humboldt University of Berlin|University of Berlin]]
|school_tradition = {{plainlist|
* [[Continental philosophy]]
* [[Post-Kantian philosophy]]
* [[Transcendental idealism]] (disputed)<ref name=iep>{{cite web|url=http://www.iep.utm.edu/schopenh|title=Arthur Schopenhauer (1788–1860) (Internet Encyclopedia of Philosophy)|access-date=12 April 2013|archive-date=30 June 2019|archive-url=https://web.archive.org/web/20190630104319/https://www.iep.utm.edu/schopenh/|url-status=live}}</ref><ref>[[Frederick C. Beiser]] reviews the commonly held position that Schopenhauer was a transcendental idealist and he rejects it: "Though it is deeply heretical from the standpoint of transcendental idealism, Schopenhauer's objective standpoint involves a form of ''[[Transcendental realism (Schopenhauer)|transcendental realism]]'', i.e. the assumption of the independent reality of the world of experience." (Beiser 2016, p. 40)</ref>
* [[Metaphysical voluntarism]]<ref name=Brit>[https://www.britannica.com/topic/voluntarism-philosophy Voluntarism (philosophy)] {{Webarchive|url=https://web.archive.org/web/20201017074231/https://www.britannica.com/topic/voluntarism-philosophy |date=17 October 2020 }} – [[Britannica.com]]</ref>
* [[Philosophical pessimism]]
}}
|main_interests   = [[Metaphysics]], [[aesthetics]], [[ethics]], [[morality]], [[psychology]]
|notable_ideas    = {{Plain list|
* [[Animal ethics]]<ref name="Puryear"/>
* [[Anthropic principle]]<ref>Arthur Schopenhauer, ''Arthur Schopenhauer: The World as Will and Presentation, Volume 1'', Routledge, 2016, p. 211: "the world [is a] mere ''presentation'', object for a subject&nbsp;..."</ref><ref>Lennart Svensson, ''Borderline: A Traditionalist Outlook for Modern Man'', Numen Books, 2015, p. 71: "[Schopenhauer] said that 'the world is our conception'. A world without a perceiver would in that case be an impossibility. But we can—he said—gain knowledge about Essential Reality for looking into ourselves, by introspection. ... This is one of many examples of the anthropic principle. The world is there for the sake of man."</ref>
* [[Criticism of religion]]
* Criticism of [[German idealism]]<ref name="WWR3">''[[The World as Will and Representation]]'', vol. 3, Ch. 50.</ref><ref name=Jacquette/>
* [[#Eternal justice|Eternal justice]]
* [[Principle of sufficient reason|Fourfold root of the principle of sufficient reason]]
* [[Hedgehog's dilemma]]
* [[Philosophical pessimism]]
* ''[[Principium individuationis]]''
* [[Arthur Schopenhauer's aesthetics|Schopenhauerian aesthetics]]
* [[Will (philosophy)|Will]] as [[thing in itself]]
* [[Will to live]]
* [[Wooden iron]]
}}
}}

'''Arthur Schopenhauer''' ({{IPAc-en|ˈ|ʃ|oʊ|p|ən|h|aʊər}} {{respell|SHOH|pən|how|ər}};<ref>{{citation|last=Wells|first=John C.|year=2008|title=Longman Pronunciation Dictionary|edition=3rd|publisher=Longman|isbn=978-1-4058-8118-0}}</ref> {{IPA|de|ˈaʁtuːɐ̯ ˈʃoːpn̩haʊɐ|lang|De-Arthur Schopenhauer2.ogg}}; 22 February 1788&nbsp;– 21 September 1860) was a [[German philosophy|German philosopher]]. He is known for his 1818 work ''[[The World as Will and Representation]]'' (expanded in 1844), which characterizes the [[Phenomenon|phenomenal]] world as the manifestation of a blind and irrational [[noumenon|noumenal]] will.<ref>{{Cite book|last=Arthur Schopenhauer|year=2004|url=https://archive.org/details/essaysaphorisms00scho/page/23|title=Essays and Aphorisms|publisher=Penguin Classics|isbn=978-0-14-044227-4|page=[https://archive.org/details/essaysaphorisms00scho/page/23 23]}}</ref><ref>{{Cite book|last=Magee|first=Bryan|title=The Philosophy of Schopenhauer|date=1997-08-14|chapter-url=https://academic.oup.com/book/32819/chapter/275011622|chapter=The World as Will|publisher=Oxford University PressOxford|isbn=978-0-19-823722-8|edition=1|pages=137–163|language=en|doi=10.1093/0198237227.003.0007|ref=none|archive-date=17 September 2023|access-date=13 September 2023|archive-url=https://web.archive.org/web/20230917173730/https://academic.oup.com/book/32819/chapter/275011622|url-status=live}}</ref><ref>{{Cite journal|last=Vandenabeele|first=Bart|date=December 2007|title=Schopenhauer on the Values of Aesthetic Experience|url=https://onlinelibrary.wiley.com/doi/10.1111/j.2041-6962.2007.tb00065.x|journal=The Southern Journal of Philosophy|language=en|volume=45|issue=4|pages=565–582|doi=10.1111/j.2041-6962.2007.tb00065.x|archive-date=17 September 2023|access-date=13 September 2023|archive-url=https://web.archive.org/web/20230917173733/https://onlinelibrary.wiley.com/doi/10.1111/j.2041-6962.2007.tb00065.x|url-status=live|url-access=subscription}}</ref> Building on the [[transcendental idealism]] of [[Immanuel Kant]], Schopenhauer developed an [[atheistic]] metaphysical and ethical system that rejected the contemporaneous ideas of [[German idealism]].<ref name="WWR3"/><ref name=Jacquette>{{cite book|title=Schopenhauer, Philosophy and the Arts|year=2007|publisher=Cambridge University Press|isbn=978-0-521-04406-6|editor=Dale Jacquette|page=162|quote=For Kant, the mathematical sublime, as seen for example in the starry heavens, suggests to imagination the infinite, which in turn leads by subtle turns of contemplation to the concept of God. Schopenhauer's atheism will have none of this, and he rightly observes that despite adopting Kant's distinction between the dynamical and mathematical sublime, his theory of the sublime, making reference to the struggles and sufferings of struggles and sufferings of Will, is unlike Kant's.}}</ref>

Schopenhauer was among the first philosophers in the [[Western philosophy|Western tradition]] to share and affirm significant tenets of [[Indian philosophy]], such as [[asceticism]], denial of the [[self (philosophy)|self]], and the notion of the [[Maya (religion)|world-as-appearance]].<ref>See the book-length study about oriental influences on the genesis of Schopenhauer's philosophy by [[Urs App]]: ''Schopenhauer's Compass. An Introduction to Schopenhauer's Philosophy and its Origins''. Wil: UniversityMedia, 2014 ({{ISBN|978-3-906000-03-9}})
* {{cite book|title=An Introduction to the History of Psychology|edition=6th|year=2009|publisher=Cengage Learning|isbn=978-0-495-50621-8|last=Hergenhahn |first=B. R.|page=216|quote=Although Schopenhauer was an atheist, he realized that his philosophy of denial had been part of several great religions; for example, Christianity, Hinduism, and Buddhism.}}<!--|access-date=2 September 2012--></ref> His work has been described as an exemplary manifestation of [[philosophical pessimism]].<ref>{{cite book|title=Essays and Aphorisms|year=2004|publisher=Penguin Classics|isbn=978-0-14-044227-4|author=Arthur Schopenhauer|pages=[https://archive.org/details/essaysaphorisms00scho/page/22 22–36]|quote=...but there has been none who tried with so great a show of learning to demonstrate that the pessimistic outlook is ''justified'', that life itself is really bad. It is to this end that Schopenhauer's metaphysic of will and idea exists.|url=https://archive.org/details/essaysaphorisms00scho/page/22}}
* ''[http://librivox.org/studies-in-pessimism-by-arthur-schopenhauer/ Studies in Pessimism] {{Webarchive|url=https://web.archive.org/web/20080415161707/http://librivox.org/studies-in-pessimism-by-arthur-schopenhauer/ |date=15 April 2008 }}'' – audiobook from [[LibriVox]].
* {{cite book|title=Encyclopedia of Psychology and Religion, Volume 2|year=2009|publisher=Springer|isbn=978-0-387-71801-9|editor1=David A. Leeming |editor2=Kathryn Madden |editor3=Stanton Marlan |page=824|quote=A more accurate statement might be that for a German—rather than a French or British writer of that time—Schopenhauer was an honest and open atheist.}}<!--|access-date=2 September 2012--></ref> Though his work failed to garner substantial attention during his lifetime, he had a posthumous impact across various disciplines, including [[philosophy]], literature, and science. His writing on [[Schopenhauer's aesthetics|aesthetics]], [[morality]] and [[psychology]] has influenced many thinkers and artists.

== Early life ==
[[File:Gdansk Schopenhauer House.jpg|thumb|upright|Schopenhauer's birthplace house, Ul. Św. Ducha (formerly ''Heiliggeistgasse'')]]
Arthur Schopenhauer was born on 22 February 1788, in [[Danzig]] (then part of the [[Polish–Lithuanian Commonwealth]]; present-day Gdańsk, Poland) on Heiliggeistgasse (present-day Św. Ducha 47), the son of {{ill|Heinrich Floris Schopenhauer|de}} and his wife [[Johanna Schopenhauer]] (née Trosiener),<ref name="Google Books">{{cite book|url=https://books.google.com/books?id=fW5Dl-tUS_oC&q=Schopenhauer+%2222+February%22&pg=PR30|title=Chronology|last=Schopenhauer|first=Arthur|author2=Günter Zöller|author3=Eric F. J. Payne|year=1999|series=Prize Essay on the Freedom of the Will|publisher=[[Cambridge University Press]]|page=xxx |isbn=978-0-521-57766-3}}</ref> both descendants of wealthy German [[Patrician (post-Roman Europe)|patrician]] families. While they both came from [[Protestantism|Protestant]] backgrounds, they were not very religious;{{r|Cartwright-2010|p=79}}<ref name="Bullock 1920 p. 53">{{cite book | last=Bullock | first=A.B. | title=The Supreme Human Tragedy: And Other Essays | publisher=C.W. Daniel | year=1920 | url=https://books.google.com/books?id=XipHAAAAMAAJ&pg=PA53 | access-date=2022-10-22 | page=53}}</ref> they supported the [[French Revolution]]{{r|Cartwright-2010|p=13}} and were [[Republicanism|republicans]], [[Cosmopolitanism|cosmopolitans]], and [[Anglophilia|Anglophiles]].{{r|Cartwright-2010|p=9}} When Danzig became part of [[Kingdom of Prussia|Prussia]] in 1793, Heinrich moved to [[Hamburg]]—a free city with a republican constitution. His firm continued trading in Danzig, where most of their extended families remained. [[Adele Schopenhauer|Adele]], Arthur's only sibling, was born on 12 July 1797.{{Citation needed|date=September 2024}}

In 1797, Arthur was sent to [[Le Havre]] to live with the family of his father's business associate, Grégoire de Blésimaire. He seemed to enjoy his two-year stay there, learning to speak French and fostering a life-long friendship with Jean Anthime Grégoire de Blésimaire.{{r|Cartwright-2010|p=18}} As early as 1799, Arthur started playing the flute.{{r|Cartwright-2010|p=30}}

In 1803 he accompanied his parents on a European tour of [[Netherlands|Holland]], [[United Kingdom of Great Britain and Ireland|Britain]], France, [[Switzerland]], [[Austria]] and [[Prussia]]. Viewed as primarily a pleasure tour, Heinrich used the opportunity to visit some of his business associates abroad.{{Citation needed|date=September 2024}}

Heinrich presented Arthur with a choice: he could either stay at home to begin preparations for university or travel with them to further his merchant education. Arthur chose to travel with them. He deeply regretted his choice later because the merchant training was very tedious. He spent twelve weeks of the tour attending school in [[Wimbledon, London]], where he was confused by strict and intellectual [[Anglicanism|Anglicans]], whom he described as shallow. He continued to sharply criticize Anglican religiosity later in life despite his general Anglophilia.{{r|Cartwright-2010|p=56}} He was also under pressure from his father, who became very critical of his educational results.{{Citation needed|date=September 2024}}

In 1805 Heinrich drowned in a canal near their home in Hamburg. Although it was possible that his death was accidental, his wife and son believed that it was suicide. He was prone to [[anxiety]] and [[Major depressive disorder|depression]], each becoming more pronounced later in his life.<ref>Safranski (1990), p. 12</ref> Heinrich had become so fussy that even his wife started to doubt his mental health.{{r|Cartwright-2010|p=43}} "There was, in the father's life, some dark and vague source of fear which later made him hurl himself to his death from the attic of his house in Hamburg."{{r|Cartwright-2010|p=88}}

Arthur showed similar moodiness during his youth and often acknowledged that he inherited it from his father. There were other instances of serious mental health problems on his father's side of the family.{{r|Cartwright-2010|p=4}} Despite his hardship, Schopenhauer liked his father and later referred to him in a positive light.{{r|Cartwright-2010|p=90}} Heinrich left the family with a significant inheritance split in three among Johanna and the children. Arthur was entitled to control of his part when he reached the age of majority. He invested it conservatively in government bonds and earned annual interest that was more than double the salary of a university professor.{{r|Cartwright-2010|p=136}} After quitting his merchant apprenticeship, with encouragement from his mother, he dedicated himself to studies at the [[Ernestine Gymnasium, Gotha]], in [[Saxe-Gotha-Altenburg]]. While there, he also enjoyed a social life among the local nobility, spending large amounts of money, which deeply concerned his frugal mother.{{r|Cartwright-2010|p=128}} He left the Gymnasium after writing a satirical poem about one of the schoolmasters. Although Arthur claimed he left voluntarily, his mother's letter indicates that he may have been expelled.{{r|Cartwright-2010|p=129}}

[[File:ArthurSchopenhauer.jpg|thumb|left|Schopenhauer in his youth]]
Arthur spent two years as a merchant in honor of his dead father. During this time, he had doubts about being able to start a new life as a scholar.{{r|Cartwright-2010|p=120}} Most of his prior education was as a practical merchant and he had trouble learning Latin; a prerequisite for an academic career.{{r|Cartwright-2010|p=117}}

His mother moved away, with her daughter Adele, to [[Weimar]]—then the center of [[German literature]]—to enjoy social life among writers and artists. Arthur and his mother did not part on good terms. In one letter, she wrote: "You are unbearable and burdensome, and very hard to live with; all your good qualities are overshadowed by your conceit, and made useless to the world simply because you cannot restrain your propensity to pick holes in other people."<ref>{{cite book |last= Wallace|first= W.|date= 2003|title= Life of Arthur Schopenhauer|location= Honolulu|publisher= University Press of the Pacific|page= 59|isbn=978-1-4102-0641-1}}</ref> His mother, Johanna, was generally described as vivacious and sociable.{{r|Cartwright-2010|p=9}} She died 24 years later.

Arthur moved to Hamburg to live with his friend Jean Anthime, who was also studying to become a merchant.

== Education ==
He moved to Weimar but did not live with his mother, who even tried to discourage him from coming by explaining that they would not get along very well.{{r|Cartwright-2010|p=131}} Their relationship deteriorated even further due to their temperamental differences. He accused his mother of being financially irresponsible, flirtatious and seeking to remarry, which he considered an insult to his father's memory.{{r|Cartwright-2010|p=116,131}} His mother, while professing her love to him, criticized him sharply for being moody, tactless, and argumentative, and urged him to improve his behavior so that he would not alienate people.{{r|Cartwright-2010|p=129}} Arthur concentrated on his studies, which were now going very well, and he also enjoyed the usual social life such as balls, parties and theater. By that time Johanna's famous salon was well established among local intellectuals and dignitaries, the most celebrated of them being [[Johann Wolfgang von Goethe]]. Arthur attended her parties, usually when he knew that Goethe would be there—although the famous writer and statesman seemed not even to notice the young and unknown student. It is possible that Goethe kept a distance because Johanna warned him about her son's depressive and combative nature, or because Goethe was then on bad terms with Arthur's language instructor and roommate, [[Franz Passow]].{{r|Cartwright-2010|p=134}} Schopenhauer was also captivated by [[Karoline Jagemann]], whom he found beautiful and who was the mistress of [[Karl August, Grand Duke of Saxe-Weimar-Eisenach]], and he wrote to her his only known love poem.{{r|Cartwright-2010|p=135}} Despite his later celebration of asceticism and negative views of sexuality, Schopenhauer occasionally had sexual affairs—usually with women of lower social status, such as servants, actresses and sometimes prostitutes.{{r|Cartwright-2010|p=21}} In a letter to his friend Anthime he claims that such affairs continued even in his mature age and admits that he had two out-of-wedlock daughters (born in 1819 and 1836), both of whom died in infancy.{{r|Cartwright-2010|p=25}} In their youthful correspondence Arthur and Anthime were somewhat boastful and competitive about their sexual exploits—but Schopenhauer seemed aware that women usually did not find him very charming or physically attractive, and his desires often remained unfulfilled.{{r|Cartwright-2010|p=22}}

He left Weimar to become a student at the [[Georg August University of Göttingen|University of Göttingen]] in 1809. There are no written reasons about why Schopenhauer chose that university instead of the then more famous [[University of Jena]], but Göttingen was known as more modern and scientifically oriented, with less attention given to theology.{{r|Cartwright-2010|p=140}} Law or medicine were usual choices for young men of Schopenhauer's status who also needed a career and income; he chose medicine due to his [[Natural science|scientific interests]]. Among his notable professors were [[Bernhard Friedrich Thibaut]], [[Arnold Hermann Ludwig Heeren]], [[Johann Friedrich Blumenbach]], [[Friedrich Stromeyer]], [[Heinrich Adolf Schrader]], [[Johann Tobias Mayer]] and [[Konrad Johann Martin Langenbeck]].{{r|Cartwright-2010|p=141–144}} He studied [[metaphysics]], [[psychology]] and [[logic]] under [[Gottlob Ernst Schulze]], the author of ''[[Aenesidemus (book)|Aenesidemus]]'', who made a strong impression and advised him to concentrate on [[Plato]] and [[Immanuel Kant]].{{r|Cartwright-2010|p=144}} He decided to switch from medicine to philosophy around 1810–11 and he left Göttingen, which did not have a strong philosophy program: besides Schulze, the only other philosophy professor was [[Friedrich Bouterwek]], whom Schopenhauer disliked.{{r|Cartwright-2010|p=150}} He did not regret his medicinal and scientific studies; he claimed that they were necessary for a philosopher, and even in Berlin he attended more lectures in sciences than in philosophy.{{r|Cartwright-2010|p=170}} During his days at Göttingen, he spent considerable time studying, but also continued his flute playing and social life. His friends included [[Friedrich Gotthilf Osann]], [[Karl Witte]], [[Christian Charles Josias von Bunsen]] and [[William Backhouse Astor Sr.]]{{r|Cartwright-2010|p=151}}

He arrived at the newly founded [[University of Berlin]] for the winter semester of 1811–12. At the same time, his mother had just begun her literary career; she published her first book in 1810, a biography of her friend [[Karl Ludwig Fernow]], which was a critical success. Arthur attended lectures by the prominent [[post-Kantian]] philosopher [[Johann Gottlieb Fichte]], but quickly found many points of disagreement with his epistemology; he also found Fichte's lectures tedious and hard to understand.{{r|Cartwright-2010|p=159}} He later mentioned Fichte only in critical, negative terms{{r|Cartwright-2010|p=159}}—seeing his philosophy as a lower-quality version of Kant's and considering it useful only because Fichte's poor arguments unintentionally highlighted some failings of Kantianism.{{r|Cartwright-2010|pages=165–169}} He also attended the lectures of the famous Protestant theologian [[Friedrich Schleiermacher]], whom he also quickly came to dislike.{{r|Cartwright-2010|p=174}} His notes and comments on Schleiermacher's lectures show that Schopenhauer was becoming very [[Criticism of religion|critical of religion]] and moving towards [[atheism]].{{r|Cartwright-2010|p=175}} He learned by self-directed reading; besides Plato, Kant and Fichte he also read the works of [[Friedrich Wilhelm Joseph Schelling]], [[Jakob Friedrich Fries]], [[Friedrich Heinrich Jacobi]], [[Francis Bacon]], [[John Locke]] and much current scientific literature.{{r|Cartwright-2010|p=170}} He attended philological courses by [[August Böckh]] and [[Friedrich August Wolf]] and continued his naturalistic interests with courses by [[Martin Heinrich Klaproth]], [[Paul Erman]], [[Johann Elert Bode]], [[Ernst Gottfried Fischer]], [[Johann Horkel]], [[Friedrich Christian Rosenthal]] and [[Hinrich Lichtenstein]] (Lichtenstein was also a friend whom he met at one of his mother's parties in Weimar).{{r|Cartwright-2010|p=171–174}}

== Early work ==
Schopenhauer left Berlin in a rush in 1813, fearing that the city could be attacked and that he could be pressed into military service as Prussia had just joined the [[War of the Sixth Coalition|war against France]].{{r|Cartwright-2010|p=179}} He returned to Weimar but left after less than a month, disgusted by the fact that his mother was now living with her supposed lover, {{ill|Georg Friedrich Konrad Ludwig Müller von Gerstenbergk|de|Georg Friedrich von Gerstenbergk}}, a civil servant twelve years younger than she; he considered the relationship an act of infidelity to his father's memory.{{r|Cartwright-2010|p=188}} He settled for a while in [[Rudolstadt]], hoping that no army would pass through the small town. He spent his time in solitude, hiking in the mountains and the [[Thuringian Forest]] and writing his dissertation, ''[[On the Fourfold Root of the Principle of Sufficient Reason]]''.

Schopenhauer completed his dissertation at about the same time as the French army was defeated at the [[Battle of Leipzig]]. He became irritated by the arrival of soldiers in the town and accepted his mother's invitation to visit her in Weimar. She tried to convince him that her relationship with Gerstenbergk was platonic and that she had no intention of remarrying.{{r|Cartwright-2010|p=230}} But Schopenhauer remained suspicious and often came in conflict with Gerstenbergk because he considered him untalented, pretentious, and [[German nationalism|nationalistic]].{{r|Cartwright-2010|p=231}} His mother had just published her second book, ''Reminiscences of a Journey in the Years 1803, 1804, and 1805'', a description of their family tour of Europe, which quickly became a hit. She found his dissertation incomprehensible and said it was unlikely that anyone would ever buy a copy. In a fit of temper Arthur told her that people would read his work long after the "rubbish" she wrote was totally forgotten.<ref>{{cite web |url=http://courseweb.stthomas.edu/paschons/language_http/essays/Schopenhauer.html |title=Schopenhauer: A Pessimist in the Optimistic Month of May |publisher=Germanic American Institute |access-date=12 March 2010 |archive-url=https://web.archive.org/web/20100611051923/http://courseweb.stthomas.edu/paschons/language_http/essays/schopenhauer.html |archive-date=11 June 2010  }}</ref><ref>{{cite web|url=https://archive.org/stream/selectedessaysof033377mbp/selectedessaysof033377mbp_djvu.txt |title=Full text of "Selected Essays Of Schopenhauer" |access-date=12 March 2010}}</ref> In fact, although they considered her novels of dubious quality, the [[F.A. Brockhaus AG|Brockhaus publishing firm]] held her in high esteem because they consistently sold well. Hans Brockhaus later claimed that his predecessors "saw nothing in this manuscript, but wanted to please one of our best-selling authors by publishing her son's work. We published more and more of her son Arthur's work and today nobody remembers Johanna, but her son's works are in steady demand and contribute to Brockhaus' reputation."<ref name=mom>{{citation |last=Fredriksson |first=Einar H. |contribution=The Dutch Publishing Scene: Elsevier and North-Holland |pages=61–76 |title=A Century of Science Publishing: A Collection of Essays |url=https://books.google.com/books?id=mwWrRYyck6AC&pg=PA61 |editor-last=Fredriksson |editor-first=Einar H. |display-editors=0 |isbn=978-4-274-90424-0 |publisher=IOS Press |location=Amsterdam |year=2001 }}</ref> He kept large portraits of the pair in his office in [[Leipzig]] for the edification of his new editors.<ref name=mom/>

Also contrary to his mother's prediction, Schopenhauer's dissertation made an impression on Goethe, to whom he sent it as a gift.{{r|Cartwright-2010|p=241}} Although it is doubtful that Goethe agreed with Schopenhauer's philosophical positions, he was impressed by his intellect and extensive scientific education.{{r|Cartwright-2010|p=243}} Their subsequent meetings and correspondence were a great honor to a young philosopher, who was finally acknowledged by his intellectual hero. They mostly discussed Goethe's newly published (and somewhat lukewarmly received) work on [[Theory of Colours|colour theory]]. Schopenhauer soon started writing his own treatise on the subject, ''[[On Vision and Colors]]'', which in many points differed from his teacher's. Although they remained polite towards each other, their growing theoretical disagreements—and especially Schopenhauer's extreme self-confidence and tactless criticisms—soon made Goethe become distant again and after 1816 their correspondence became less frequent.{{r|Cartwright-2010|p=247–265}} Schopenhauer later admitted that he was greatly hurt by this rejection, but he continued to praise Goethe, and considered his color theory a great introduction to his own.{{r|Cartwright-2010|p=252,256,265}}

Another important experience during his stay in Weimar was his acquaintance with Friedrich Majer<ref>{{Cite journal |last=Willson |first=A. Leslie |title=Friedrich Majer: Romantic Indologist |date=1961 |journal=Texas Studies in Literature and Language |volume=3 |issue=1 |pages=40–49 |jstor=40753707 |issn=0040-4691}}</ref>—a [[historian of religion]], [[Oriental studies|orientalist]] and disciple of [[Johann Gottfried Herder]]—who introduced him to [[Eastern philosophy]]{{sfn|Clarke|1997|pages=67–68}}{{r|Cartwright-2010|p=266}} (see also [[#Indology|Indology]]). Schopenhauer was immediately impressed by the ''[[Upanishads]]'' (he called them "the production of the highest human wisdom", and believed that they contained superhuman concepts) and the [[Buddha]],{{sfn|Clarke|1997|pages=67–68}} and put them on a par with Plato and Kant.{{r|Cartwright-2010|p=268,272}} He continued his studies by reading the ''[[Bhagavad Gita]]'', an amateurish German journal ''Asiatisches Magazin'', and ''Asiatick Researches'' by [[the Asiatic Society]].{{r|Cartwright-2010|p=267,272}} Schopenhauer held a profound respect for [[Indian philosophy]];{{sfn|Clarke|1997|pages=67–69}} and loved [[Hindu texts]]. Although he never revered a Buddhist text he regarded [[Buddhism]] as the most distinguished religion.{{sfn|Clarke|1997|pages=273}}{{r|Cartwright-2010|p=272}} His studies on Hindu and Buddhist texts were constrained by the lack of adequate literature,{{sfn|Clarke|1997|page=69}} and the latter were mostly restricted to [[Theravada Buddhism]]. He also claimed that he formulated most of his ideas independently,{{sfn|Clarke|1997|pages=67–68}} and only later realized the similarities with Buddhism.{{r|Cartwright-2010|p=274–276}}

Schopenhauer read the Latin translation and praised the Upanishads in his main work, ''[[The World as Will and Representation]]'' (1819), as well as in his ''[[Parerga and Paralipomena]]'' (1851), and commented:

<blockquote>In the whole world there is no study so beneficial and so elevating as that of the Upanishads. It has been the solace of my life, it will be the solace of my death.<ref>{{Cite book|last=Schopenhauer |first=Arthur |title=The world as will and idea|date=22 April 2019|publisher=Classic Wisdom Reprint |isbn=978-1-950330-23-2|oclc=1229105608}}</ref></blockquote>

[[Image:Arthur Schopenhauer Portrait by Ludwig Sigismund Ruhl 1815.jpeg|thumb|Schopenhauer in 1815. Portrait by [[Ludwig Sigismund Ruhl]].]]
As the relationship with his mother fell to a new low, in May 1814 he left Weimar and moved to [[Dresden]].{{r|Cartwright-2010|p=265}} He continued his philosophical studies, enjoyed the cultural life, socialized with intellectuals and engaged in sexual affairs.{{r|Cartwright-2010|p=284}} His friends in Dresden were [[Johann Gottlob von Quandt]], [[Friedrich Laun]], [[Karl Christian Friedrich Krause]] and Ludwig Sigismund Ruhl, a young painter who made a romanticized portrait of him in which he improved some of Schopenhauer's unattractive physical features.{{r|Cartwright-2010|p=278,283}} His criticisms of local artists occasionally caused public quarrels when he ran into them in public.{{r|Cartwright-2010|p=282}} Schopenhauer's main occupation during his stay in Dresden was his seminal philosophical work, ''[[The World as Will and Representation]]'', which he started writing in 1814 and finished in 1818.<ref>Although the first volume was published by December 1818, it was printed with a title page erroneously giving the year as 1819 (see {{citation |last=Braunschweig |first=Yael |contribution=Schopenhauer and Rossinian Universiality: On the Italianate in Schopenhauer's Metaphysics of Music <!--|pages=283–304--> |title=The Invention of Beethoven and Rossini: Historiography, Analysis, Criticism |url=https://books.google.com/books?id=NQ_3AQAAQBAJ |editor-last=Mathew |editor-first=Nicholas |editor2-last=Walton |editor2-first=Benjamin |display-editors=0 |publisher=Cambridge University Press |location=[[Cambridge, England|Cambridge]] |date=2013 |isbn=978-0-521-76805-4 |page=[https://books.google.com/books?id=NQ_3AQAAQBAJ&pg=PA297 297, n. 7]}}).</ref> He was recommended to the publisher [[Friedrich Arnold Brockhaus]] by Baron Ferdinand von Biedenfeld, an acquaintance of his mother.{{r|Cartwright-2010|p=285}} Although Brockhaus accepted his manuscript, Schopenhauer made a poor impression because of his quarrelsome and fussy attitude, as well as very poor sales of the book after it was published in December 1818.{{r|Cartwright-2010|p=285–289}}

In September 1818, while waiting for his book to be published and conveniently escaping an affair with a maid that caused an unwanted pregnancy,{{r|Cartwright-2010|p=342}} Schopenhauer left Dresden for a year-long vacation in Italy.{{r|Cartwright-2010|p=346}} He visited [[Venice]], [[Bologna]], [[Florence]], [[Naples]] and [[Milan]], travelling alone or accompanied by mostly English tourists he met.{{r|Cartwright-2010|p=350}} He spent the winter months in Rome, where he accidentally met his acquaintance [[Karl Witte]] and engaged in numerous quarrels with German tourists in the [[Antico Caffè Greco|Caffè Greco]], among them [[Johann Friedrich Böhmer]], who also mentioned his insulting remarks and unpleasant character.{{r|Cartwright-2010|p=348–349}} He enjoyed art, architecture, and ancient ruins, attended plays and operas, and continued his philosophical contemplation and love affairs.{{r|Cartwright-2010|p=346–350}} One of his affairs supposedly became serious, and for a while he contemplated marriage to a rich Italian noblewoman—but, despite his mentioning this several times, no details are known and it may have been Schopenhauer exaggerating.<ref>Safranski, Rüdiger (1991) ''[[Schopenhauer and the Wild Years of Philosophy]]''. Harvard University Press. p. 244</ref>{{r|Cartwright-2010|p=345}} He corresponded regularly with his sister Adele and became close to her as her relationship with Johanna and Gerstenbergk also deteriorated.{{r|Cartwright-2010|p=344}} She informed him about their financial troubles as the banking house of A. L. Muhl in Danzig—in which her mother invested their whole savings and Arthur a third of his—was near bankruptcy.{{r|Cartwright-2010|p=351}} Arthur offered to share his assets, but his mother refused and became further enraged by his insulting comments.{{r|Cartwright-2010|p=352}} The women managed to receive only thirty percent of their savings while Arthur, using his business knowledge, took a suspicious and aggressive stance towards the banker and eventually received his part in full.{{r|Cartwright-2010|p=354–356}} The affair additionally worsened the relationships among all three members of the Schopenhauer family.{{r|Cartwright-2010|p=352,354}}

He shortened his stay in Italy because of the trouble with Muhl and returned to Dresden.{{r|Cartwright-2010|p=356}} Disturbed by the financial risk and the lack of responses to his book he decided to take an academic position since it provided him with both income and an opportunity to promote his views.{{r|Cartwright-2010|p=358}} He contacted his friends at universities in Heidelberg, Göttingen and Berlin and found [[Humboldt University of Berlin|Berlin]] most attractive.{{r|Cartwright-2010|p=358–362}} He scheduled his lectures to coincide with those of the famous philosopher [[Georg Wilhelm Friedrich Hegel]], whom Schopenhauer described as a "clumsy charlatan".<ref>Schopenhauer, Arthur. Author's preface to "On the Fourfold Root of the Principle of sufficient reason", p.&nbsp;1 ([[:s:On the Fourfold Root of the Principle of Sufficient Reason|On the Fourfold Root of the Principle of Sufficient Reason]] on Wikisource.)</ref> He was especially appalled by Hegel's supposedly poor knowledge of natural sciences and tried to engage him in a quarrel about it already at his test lecture in March 1820.{{r|Cartwright-2010|p=363}} Hegel was also facing political suspicions at the time, when many progressive professors were dismissed following the [[Carlsbad Decrees]], while Schopenhauer carefully mentioned in his application that he had no interest in politics.{{r|Cartwright-2010|p=362}} Despite their differences and the arrogant request to schedule lectures at the same time as his own, Hegel still voted to accept Schopenhauer to the university.{{r|Cartwright-2010|p=365}} Only five students turned up to Schopenhauer's lectures, and he dropped out of academia. A late essay, "On University Philosophy", expressed his resentment towards the work conducted in academies.

== Later life ==
[[File:Arthur Shopengauer by Gennadij Jerszow.jpg|thumb|upright|Sculpture of Arthur Schopenhauer by [[Giennadij Jerszow]]]]
After his efforts in academia, he continued to travel extensively, visiting [[Leipzig]], [[Nuremberg]], [[Stuttgart]], [[Schaffhausen]], [[Vevey]], Milan and spending eight months in Florence.{{r|Cartwright-2010|p=411}} Before he left for his three-year travel, Schopenhauer had an incident with his Berlin neighbor, 47-year-old seamstress Caroline Louise Marquet. The details of the August 1821 incident are unknown. He claimed that he had just pushed her from his entrance after she had rudely refused to leave, and that she had purposely fallen to the ground so that she could sue him. She claimed that he had attacked her so violently that she had become paralyzed on her right side and unable to work. She immediately sued him, and the court case lasted until May 1827, when a court found Schopenhauer guilty and forced him to pay her an annual pension until her death in 1842.{{r|Cartwright-2010|p=408–411}}

Schopenhauer enjoyed Italy, where he studied art and socialized with Italian and English nobles.{{r|Cartwright-2010|p=411–414}} It was his last visit to the country. He left for [[Munich]] and stayed there for a year, mostly recuperating from various health problems, some of them possibly caused by venereal diseases (the treatment his doctor used suggests [[syphilis]]).{{r|Cartwright-2010|p=415}} He contacted publishers, offering to translate Hume into German and Kant into English, but his proposals were declined.{{r|Cartwright-2010|p=417,422}} Returning to Berlin, he began to study Spanish so he could read some of his favorite authors in their original language. He liked [[Pedro Calderón de la Barca]], [[Lope de Vega]], [[Miguel de Cervantes]] and especially [[Baltasar Gracián]].{{r|Cartwright-2010|p=420}} He also made failed attempts to publish his translations of their works. A few attempts to revive his lectures—again scheduled at the same time as Hegel's—also failed, as did his inquiries about relocating to other universities.{{r|Cartwright-2010|p=429–432}}

During his Berlin years, Schopenhauer occasionally mentioned his desire to marry and have a family.{{r|Cartwright-2010|p=404,432}} For a while he was unsuccessfully courting 17-year-old Flora Weiss, who was 22 years younger than he was.{{r|Cartwright-2010|p=433}} His unpublished writings from that time show that he was already very critical of [[monogamy]] but still not advocating [[polygyny]]—instead musing about a [[Polyamory|polyamorous]] relationship that he called "tetragamy".{{r|Cartwright-2010|p=404–408}} He had an on-and-off relationship with a young dancer, [[Caroline Medon|Caroline Richter]] (she also used the surname Medon after one of her ex-lovers).{{r|Cartwright-2010|p=403}} They met when he was 33 and she was 19 and working at the Berlin Opera. She had already had numerous lovers and a son out of wedlock, and later gave birth to another son, this time to an unnamed foreign diplomat (she soon had another pregnancy but the child was stillborn).{{r|Cartwright-2010|p=403–404}} As Schopenhauer was preparing to escape from Berlin in 1831, due to a [[cholera]] epidemic, he offered to take her with him on the condition that she leave her young son behind.{{r|Cartwright-2010|p=404}} She refused and he went alone; in his will he left her a significant sum of money, but insisted that it should not be spent in any way on her second son.{{r|Cartwright-2010|p=404}}

Schopenhauer claimed that, in his last year in Berlin, he had a [[wikt:premonition|prophetic dream]] that urged him to escape from the city.{{r|Cartwright-2010|p=436}} As he arrived in his new home in [[Frankfurt]], he supposedly had another [[Supernatural|supernatural experience]], an apparition of his dead father and his mother, who was still alive.{{r|Cartwright-2010|p=436}} This experience led him to spend some time investigating [[paranormal]] phenomena and [[Magic (supernatural)|magic]]. He was quite critical of the available studies and claimed that they were mostly ignorant or fraudulent, but he did believe that there are authentic cases of such phenomena and tried to explain them through his metaphysics as manifestations of the will.{{r|Cartwright-2010|p=437–452}}

Upon his arrival in Frankfurt, he experienced a period of depression and declining health.{{r|Cartwright-2010|p=454}} He renewed his correspondence with his mother, and she seemed concerned that he might commit suicide like his father.{{r|Cartwright-2010|p=454–457}} By now Johanna and Adele were living very modestly. Johanna's writing did not bring her much income, and her popularity was waning.{{r|Cartwright-2010|p=458}} Their correspondence remained reserved, and Arthur seemed undisturbed by her death in 1838.{{r|Cartwright-2010|p=460}} His relationship with his sister grew closer and he corresponded with her until she died in 1849.{{r|Cartwright-2010|p=463}}

In July 1832, Schopenhauer left Frankfurt for [[Mannheim]] but returned in July 1833 to remain there for the rest of his life, except for a few short journeys.{{r|Cartwright-2010|p=464}} He lived alone except for a succession of pet [[poodle]]s named [[Ātman (Hinduism)|Atman]] and Butz. In 1836, he published ''On the Will in Nature''. In 1838, he sent his essay "[[On the Freedom of the Will]]" to the contest of the [[Royal Norwegian Society of Sciences and Letters|Royal Norwegian Society of Sciences]] in 1838 and won the prize in 1839. He sent another essay, "[[On the Basis of Morality]]", to the [[Royal Danish Academy of Sciences and Letters|Royal Danish Society of Sciences]] in 1839, but did not win the (1840) prize despite being the only contestant.<ref>{{harvnb|Schopenhauer|2010|pages=xxviii, xxxix}}</ref> The Society was appalled that several distinguished contemporary philosophers were mentioned in a very offensive manner, and claimed that the essay missed the point of the set topic and that the arguments were inadequate.{{r|Cartwright-2010|p=483}} Schopenhauer, who had been very confident that he would win, was enraged by this rejection. He published both essays as ''The Two Basic Problems of Ethics''. The first edition, published September 1840 but with an 1841 date, again failed to draw attention to his philosophy. In the preface to the second edition, in 1860, he was still pouring insults on the Royal Danish Society.{{r|Cartwright-2010|p=484}} Two years later, after some negotiations, he managed to convince his publisher, Brockhaus, to print the second, updated edition of ''The World as Will and Representation''. That book was again mostly ignored and the few reviews were mixed or negative.

Schopenhauer began to attract some followers, mostly outside academia, among practical professionals (several of them were lawyers) who pursued private philosophical studies. He jokingly referred to them as "evangelists" and "apostles".{{r|Cartwright-2010|p=504}} One of the most active early followers was [[Julius Frauenstädt]], who wrote numerous articles promoting Schopenhauer's philosophy. He was also instrumental in finding another publisher after Brockhaus declined to publish ''Parerga and Paralipomena'', believing that it would be another failure.{{r|Cartwright-2010|p=506}} Though Schopenhauer later stopped corresponding with him, claiming that he did not adhere closely enough to his ideas, Frauenstädt continued to promote Schopenhauer's work.{{r|Cartwright-2010|p=507–508}} They renewed their communication in 1859 and Schopenhauer named him heir for his literary estate.{{r|Cartwright-2010|p=508}} Frauenstädt also became the editor of the first collected works of Schopenhauer.{{r|Cartwright-2010|p=506}}

In 1848, Schopenhauer witnessed a [[German revolutions of 1848–1849|violent upheaval]] in Frankfurt after General [[Hans Adolf Erdmann von Auerswald]] and Prince [[Felix Lichnowsky]] were murdered. He became worried for his own safety and property.{{r|Cartwright-2010|p=514}} Even earlier in life he had had such worries and kept a sword and loaded pistols near his bed to defend himself from thieves.{{r|Cartwright-2010|p=465}} He gave a friendly welcome to Austrian soldiers who wanted to shoot revolutionaries from his window and as they were leaving he gave one of the officers his opera glasses to help him monitor rebels.{{r|Cartwright-2010|p=514}} The rebellion passed without any loss to Schopenhauer and he later praised [[Alfred I, Prince of Windisch-Grätz]], for restoring order.{{r|Cartwright-2010|p=515}} He even modified his will, leaving a large part of his property to a Prussian fund that helped soldiers who became invalids while fighting rebellion in 1848 or the families of soldiers who died in battle.{{r|Cartwright-2010|p=517}} As [[Young Hegelians]] were advocating change and progress, Schopenhauer claimed that misery is natural for humans and that, even if some utopian society were established, people would still fight each other out of boredom, or would starve due to overpopulation.{{r|Cartwright-2010|p=515}}

[[File:Schopenhauer by Jules Lunteschütz.jpg|thumb|left|1855 painting of Schopenhauer by [[Jules Lunteschütz]]]]
In 1851, Schopenhauer published ''[[Parerga and Paralipomena]]'', which contains essays that are supplementary to his main work. It was his first successful, widely read book, partly due to the work of his disciples who wrote praising reviews.{{r|Cartwright-2010|p=524}} The essays that proved most popular were the ones that actually did not contain the basic philosophical ideas of his system.{{r|Cartwright-2010|p=539}} Many academic philosophers considered him a great stylist and cultural critic but did not take his philosophy seriously.{{r|Cartwright-2010|p=539}} His early critics liked to point out similarities of his ideas to those of Fichte and Schelling,{{r|Cartwright-2010|p=381–386}} or to claim that there were numerous contradictions in his philosophy.{{r|Cartwright-2010|p=381–386, 537}} Both criticisms enraged Schopenhauer. He was becoming less interested in intellectual fights, but encouraged his disciples to do so.{{r|Cartwright-2010|p=525}} His private notes and correspondence show that he acknowledged some of the criticisms regarding contradictions, inconsistencies, and vagueness in his philosophy, but claimed that he was not concerned about harmony and agreement in his propositions{{r|Cartwright-2010|p=394}} and that some of his ideas should not be taken literally but instead as metaphors.{{r|Cartwright-2010|p=510}}

Academic philosophers were also starting to notice his work. In 1856 the University of Leipzig sponsored an essay contest about Schopenhauer's philosophy, which was won by [[Rudolf Seydel]]'s very critical essay.{{r|Cartwright-2010|p=536}} Schopenhauer's friend [[Jules Lunteschütz]] made the first of his four portraits of him—which Schopenhauer did not particularly like—which was soon sold to a wealthy landowner, Carl Ferdinand Wiesike, who built a house to display it. Schopenhauer seemed flattered and amused by this, and would claim that it was his first chapel.{{r|Cartwright-2010|p=540}} As his fame increased, copies of paintings and photographs of him were being sold and admirers were visiting the places where he had lived and written his works. People visited Frankfurt's ''Englischer Hof'' to observe him dining. Admirers gave him gifts and asked for autographs.{{r|Cartwright-2010|p=541}} He complained that he still felt isolated due to his not very social nature and the fact that many of his good friends had already died from old age.{{r|Cartwright-2010|p=542}}

[[File:Schopenhauer-ffm001.jpg|thumb|right|Grave at the ''[[Frankfurt Main Cemetery|Hauptfriedhof]]'' in [[Frankfurt]]]]
He remained healthy in his own old age, which he attributed to regular walks no matter the weather and always getting enough sleep.{{r|Cartwright-2010|p=544–545}} He had a great appetite and could read without glasses, but his hearing had been declining since his youth and he developed problems with [[rheumatism]].{{r|Cartwright-2010|p=545}} He remained active and lucid, continued his reading, writing and correspondence until his death.{{r|Cartwright-2010|p=545}} The numerous notes that he made during these years, amongst others on aging, were published [[Posthumous publication|posthumously]] under the title ''Senilia''. In the spring of 1860 his health began to decline, and he experienced shortness of breath and heart palpitations; in September he suffered inflammation of the lungs and, although he was starting to recover, he remained very weak.{{r|Cartwright-2010|p=546}} The last friend to visit him was Wilhelm Gwinner, who said that Schopenhauer was concerned that he would not be able to finish his planned additions to ''Parerga and Paralipomena'' but was at peace with dying.{{r|Cartwright-2010|p=546–547}} He died of [[Respiratory failure|pulmonary-respiratory failure]]<ref>Dale Jacquette, ''The Philosophy of Schopenhauer'', Routledge, 2015: "Biographical sketch".</ref> on 21 September 1860 while sitting at home on his couch. He died at age 72 and had a funeral conducted by a [[Lutheranism|Lutheran]] minister.<ref>''Schopenhauer: his life and philosophy'' by H. Zimmern – 1932 – G. Allen & Unwin.</ref><ref>{{Cite book|last=Lewis|first=Peter|url=https://books.google.com/books?id=6TBXX9KVtzsC&dq=%22Arthur+Schopenhauer%22+%22lutheran%22&pg=PA167|title=Arthur Schopenhauer, 2013|date=15 February 2013|publisher=Reaktion Books|isbn=978-1-78023-069-6}}</ref>

== Philosophy ==
=== Theory of perception ===
In November 1813 [[Johann Wolfgang von Goethe]] invited Schopenhauer to help him on his [[Theory of Colours]]. Although Schopenhauer considered colour theory a minor matter,<ref>Letter to Goethe on 23 January 1816:  "Ich weiß, daß durch mich die Wahrheit geredet hat, – in dieser kleinen Sache, wie dereinst in größern."</ref> he accepted the invitation out of admiration for Goethe. Nevertheless, these investigations led him to his most important discovery in epistemology: finding a demonstration for the ''a priori'' nature of causality.

Kant openly admitted that it was [[David Hume|Hume]]'s skeptical assault on causality that motivated the critical investigations in ''[[Critique of Pure Reason]]'' and gave an elaborate proof to show that causality is ''a priori''. After [[Gottlob Ernst Schulze]] had made it plausible that Kant had not disproven Hume's skepticism, it was up to those loyal to Kant's project to prove this important matter.

The difference between the approaches of Kant and Schopenhauer was this: Kant simply declared that the empirical content of perception is "given" to us from outside, an expression with which Schopenhauer often expressed his dissatisfaction.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|volume=1. Criticism of the Kantian Philosophy|quote=But the whole teaching of Kant contains really nothing more about this than the oft-repeated meaningless expression: 'The empirical element in perception is given from without.' ... always through the same meaningless metaphorical expression: 'The empirical perception is given us.'}}</ref> He, on the other hand, was occupied with the questions: how do we get this empirical content of perception; how is it possible to comprehend subjective sensations "limited to my skin" as the objective perception of things that lie "outside" of me?<ref>{{Cite book|title=On the Fourfold Root of the Principle of Sufficient Reason|last=Schopenhauer|first=Arthur|at=§ 21|quote=For sensation is and remains a process within the organism and is limited, as such, to the region within the skin; it cannot therefore contain any thing which lies beyond that region, or, in other words, anything that is outside us. ... It is only when the Understanding begins to apply its sole form, the causal law, that a powerful transformation takes place, by which subjective sensation becomes objective perception.}}</ref>

{{Blockquote|The sensations in the hand of a man born blind, on feeling an object of cubic shape, are quite uniform and the same on all sides and in every direction: the edges, it is true, press upon a smaller portion of his hand, still nothing at all like a cube is contained in these sensations. His Understanding draws the immediate and intuitive conclusion from the resistance felt, that this resistance must have a cause, which then presents itself through that conclusion as a hard body; and through the movements of his arms in feeling the object, while the hand's sensation remains unaltered, he constructs the cubic shape in Space. If the representation of a cause and of Space, together with their laws, had not already existed within him, the image of a cube could never have proceeded from those successive sensations in his hand.<ref>{{Cite book|title=On the Fourfold Root of the Principle of Sufficient Reason|last=Schopenhauer|first=Arthur|at=§ 21}}</ref>|sign=|source=}}

Causality is therefore not an empirical concept drawn from objective perceptions, as Hume had maintained; instead, as Kant had said, objective perception presupposes knowledge of causality.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|volume=1, § 4.|quote=The contrary doctrine that the law of causality results from experience, which was the scepticism of Hume, is first refuted by this. For the independence of the knowledge of causality of all experience,—that is, its a priori character—can only be deduced from the dependence of all experience upon it; and this deduction can only be accomplished by proving, in the manner here indicated, and explained in the passages referred to above, that the knowledge of causality is included in perception in general, to which all experience belongs, and therefore in respect of experience is completely a priori, does not presuppose it, but is presupposed by it as a condition.}}</ref>

By this intellectual operation, comprehending every effect in our sensory organs as having an external cause, the external world arises. With vision, finding the cause is essentially simplified due to light acting in straight lines. We are seldom conscious of the process that interprets the double sensation in both eyes as coming from one object, that inverts the impressions on the retinas, and that uses the change in the apparent position of an object relative to more distant objects provided by binocular vision to perceive depth and distance.

Schopenhauer stresses the importance of the intellectual nature of perception; the senses furnish the raw material by which the intellect produces the world as representation. He set out his theory of perception for the first time in ''[[On Vision and Colors]]'',<ref name=":0" /> and, in the subsequent editions of ''Fourfold Root'', an extensive exposition is given in § 21.

=== World as representation ===
Schopenhauer saw his philosophy as an extension of Kant's, and used the results of Kant's theoretical and epistemological investigations ([[transcendental idealism]]) as starting point for his own. Kant had argued that the [[Empirical evidence|empirical]] world is merely a complex of appearances whose existence and connection occur only in our [[mental representation]]s.<ref>{{Cite book|title=Prolegomena to Any Future Metaphysics|last=Kant|first=Immanuel|at=§ 52c|translator-last=Paul Carus}}</ref> Schopenhauer did not deny that the external world existed and was known empirically, yet he followed Kant in claiming that our knowledge and experience of the world is always in some sense dependent on ''us''.<ref>See the quotation of Schopenhauer in {{Cite book| publisher = University of Chicago Press|isbn=978-0-226-78665-0| last = Storm| first = Jason Josephson| title = Metamodernism: The Future of Theory| location = Chicago| date = 2021|pages=36–37| url = https://books.google.com/books?id=pEQ6EAAAQBAJ}}</ref> For Schopenhauer in particular, the spatiotemporal form and causal structure of the external world are contributed to our experiences of it by the mind as it renders perceptions.<ref name="On the Fourfold Root of the Principle of Sufficient Reason, and On the Will in Nature: Two Essays (revised edition)">{{cite web |last1=Schopenhauer |first1=Arthur |title=On the Fourfold Root of the Principle of Sufficient Reason, and On the Will in Nature: Two Essays (revised edition) |url=https://www.gutenberg.org/cache/epub/50966/pg50966-images.html#Pg031 |website=gutenberg.org |publisher=Project Gutenberg |ref=p.65 |access-date=27 September 2024 |archive-date=3 December 2024 |archive-url=https://web.archive.org/web/20241203105520/https://www.gutenberg.org/cache/epub/50966/pg50966-images.html#Pg031 |url-status=live }}</ref> Schopenhauer reiterates this in the first sentence of his main work: "The world is my representation (''Die Welt ist meine Vorstellung'')". Everything that there is for cognition (the entire world) exists simply as an object in relation to a subject—a 'representation' to a subject. Everything that belongs to the world is, therefore, 'subject-dependent'. In Book One of ''The World as Will and Representation'', Schopenhauer considers the world from this angle—that is, insofar as it is representation.

Kant had previously argued that we perceive reality as something spatial and temporal not because reality is inherently spatial and temporal, but because that is how our minds operate in perceiving an object. Therefore, understanding objects in space and time represents our 'contribution' to an experience. For Schopenhauer, Kant's 'greatest service' lay in the 'differentiation between [[phenomena]] and the thing-in-itself ([[noumena]]), based on the proof that between everything and us there is always a perceiving mind.' In other words, Kant's primary achievement is to demonstrate that instead of being a blank slate where reality merely reveals its character, the mind, with sensory support, actively participates in constructing reality. Thus, Schopenhauer believed that Kant had shown that the everyday world of experience, and indeed the entire material world related to space and time, is merely 'appearance' or 'phenomena,' entirely distinct from the thing-in-itself.'<ref>{{Cite book|last=Young|first=Julian|date=2005|url=https://www.taylorfrancis.com/books/9781134328833|title=Schopenhauer|publisher=Routledge|isbn=978-1-134-32883-3|edition=1|pages=4–25|language=en|doi=10.4324/9780203022108}}</ref>

=== World as will ===
{{Main|The World as Will and Representation}}

In Book Two of ''The World as Will and Representation'', Schopenhauer considers what the world is beyond the aspect of it that appears to us—that is, the aspect of the world beyond representation, the world considered "[[thing-in-itself|in-itself]]" or "[[noumena]]", its inner essence. The very being in-itself of all things, Schopenhauer argues, is will (''Wille''). The empirical world that appears to us as representation has plurality and is ordered in a spatio-temporal framework. The world as thing in-itself must exist outside the subjective forms of space and time. Although the world manifests itself to our experience as a multiplicity of objects (the "objectivation" of the will), each element of this multiplicity has the same blind essence striving towards existence and life. Human rationality is merely a secondary phenomenon that does not distinguish humanity from the rest of nature at the fundamental, essential level. The advanced cognitive abilities of human beings, Schopenhauer argues, serve the ends of willing—an illogical, directionless, ceaseless striving that condemns the human individual to a life of suffering unredeemed by any final purpose. Schopenhauer's philosophy of the will as the essential reality behind the world as representation is often called [[Voluntarism (philosophy)|metaphysical voluntarism]].<ref name=Brit/>

For Schopenhauer, understanding the world as will leads to ethical concerns (see the [[#Ethics|ethics section below]] for further detail), which he explores in the Fourth Book of ''The World as Will and Representation'' and again in his two prize essays on ethics, ''[[On the Freedom of the Will]]'' and ''[[On the Basis of Morality]]''. No individual human actions are free, Schopenhauer argues, because they are events in the world of appearance and thus are subject to the principle of sufficient reason: a person's actions are a necessary consequence of motives and the given character of the individual human. Necessity extends to the actions of human beings just as it does to every other appearance, and thus we cannot speak of freedom of individual willing. Albert Einstein quoted the Schopenhauerian idea that "a man can ''do'' as he will, but not ''will'' as he will."<ref>Einstein, Albert (1935). ''The World as I See It'', p. 14. Snowball Publishing. {{ISBN|1-4948-7706-6}}.</ref> Yet the will as thing in-itself is free, as it exists beyond the realm of representation and thus is not constrained by any of the forms of necessity that are part of the principle of sufficient reason.

According to Schopenhauer, salvation from our miserable existence can come through the will's being "tranquillized" by the metaphysical insight that reveals individuality to be merely an illusion. The saint or 'great soul' intuitively "recognizes the whole, comprehends its essence, and finds that it is constantly passing away, caught up in vain strivings, inner conflict, and perpetual suffering".<ref>''The World as Will and Representation, Vol. 1'', §68</ref> The negation of the will, in other words, stems from the insight that the world in-itself (free from the forms of space and time) is one. [[Asceticism|Ascetic]] practices, Schopenhauer remarks, are used to aid the will's "self-abolition", which brings about a blissful, redemptive "will-less" state of emptiness that is free from striving or suffering.

=== Art and aesthetics ===
{{Main|Arthur Schopenhauer's aesthetics}}
[[File:Johannes Vermeer - Het melkmeisje - Google Art Project.jpg|thumb|In his main work, Schopenhauer praised the [[Dutch Golden Age painting|Dutch Golden Age artists]], who "directed such purely objective perception to the most insignificant objects, and set up a lasting monument of their objectivity and spiritual peace in paintings of ''[[still life]]''. The aesthetic beholder does not contemplate this without emotion."<ref>''The World as Will and Representation'', Vol. 1, §38</ref>]]
For Schopenhauer, human "willing"—desiring, craving, etc.—is at the root of [[suffering]]. A temporary way to escape this pain is through aesthetic contemplation. Here one moves away from ordinary cognizance of individual things to cognizance of eternal Platonic ''Ideas''—in other words, cognizance that is free from the service of will. In aesthetic contemplation, one no longer perceives an object of perception as something from which one is separated; rather "it is as if the object alone existed without anyone perceiving it, and one can thus no longer separate the perceiver from the perception, but the two have become one, the entirety of consciousness entirely filled and occupied by a single perceptual image".<ref>''The World as Will and Representation,'' Vol. 1, §34</ref> Subject and object are no longer distinguishable, and the ''Idea'' comes to the fore.

From this aesthetic immersion, one is no longer an individual who suffers as a result of servitude to one's individual will but, rather, becomes a "pure, will-less, painless, timeless, subject of cognition". The pure, will-less subject of cognition is cognizant only of Ideas, not individual things: this is a kind of cognition that is unconcerned with relations between objects according to the Principle of Sufficient Reason (time, space, cause and effect) and instead involves complete absorption in the object.

Art is the practical consequence of this brief aesthetic contemplation, since it attempts to depict the essence/pure Ideas of the world. Music, for Schopenhauer, is the purest form of art because it is the one that depicts the will itself without it appearing as subject to the Principle of Sufficient Reason, therefore as an individual object. According to [[Daniel Albright]], "Schopenhauer thought that [[philosophy of music|music]] was the only art that did not merely copy ideas, but actually embodied the will itself".<ref>Daniel Albright, ''Modernism and Music'', 2004, p. 39, footnote 34</ref> He deemed music a timeless, universal language comprehended everywhere, that can imbue global enthusiasm, if in possession of a significant melody.<ref name=Music >{{cite book|last=Schopenhauer|first=Arthur|title=Essays and Aphorisms|year=1970|publisher=Penguin Classics |isbn=978-0-14-044227-4|page=[https://archive.org/details/essaysaphorisms00scho/page/162 162]|url=https://archive.org/details/essaysaphorisms00scho/page/162}}</ref>

=== Mathematics ===
Schopenhauer's [[mathematical realism|realist]] views on mathematics are evident in his criticism of contemporaneous attempts to prove the [[parallel postulate]] in [[Euclidean geometry]]. Writing shortly before the discovery of [[hyperbolic geometry]] demonstrated the logical independence of the [[axiom]]—and long before the [[general theory of relativity]] revealed that it does not necessarily express a property of physical space—Schopenhauer criticized mathematicians for trying to use indirect [[concept]]s to prove what he held was directly evident from [[intuition|intuitive perception]].

{{blockquote|text=The Euclidean method of demonstration has brought forth from its own womb its most striking parody and caricature in the famous controversy over the theory of ''parallels'', and in the attempts, repeated every year, to prove the eleventh axiom (also known as the fifth postulate). The axiom asserts, and that indeed through the indirect criterion of a third intersecting line, that two lines inclined to each other (for this is the precise meaning of "less than two right angles"), if produced far enough, must meet. Now this truth is supposed to be too complicated to pass as self-evident, and therefore needs a proof; but no such proof can be produced, just because there is nothing more immediate.<ref name="ReferenceB">''[[The World as Will and Representation]]'', vol. 2, ch. 13</ref>}}

Throughout his writings,<ref>"I wanted in this way to stress and demonstrate the great difference, indeed opposition, between knowledge of perception and abstract or reflected knowledge. Hitherto this difference has received too little attention, and its establishment is a fundamental feature of my philosophy&nbsp;..." – ''The World as Will and Representation.'', vol. 2, ch. 7, p. 88 (trans. Payne)</ref> Schopenhauer criticized the logical derivation of philosophies and mathematics from mere concepts, instead of from intuitive perceptions.

{{blockquote|text=In fact, it seems to me that the logical method is in this way reduced to an absurdity. But it is precisely through the controversies over this, together with the futile attempts to demonstrate the ''directly'' certain as merely ''indirectly'' certain, that the independence and clearness of intuitive evidence appear in contrast with the uselessness and difficulty of logical proof, a contrast as instructive as it is amusing. The direct certainty will not be admitted here, just because it is no merely logical certainty following from the concept, and thus resting solely on the relation of predicate to subject, according to the principle of contradiction. But that eleventh axiom regarding parallel lines is a [[synthetic proposition]] ''[[A priori and a posteriori|a priori]]'', and as such has the guarantee of pure, not empirical, perception; this perception is just as immediate and certain as is the [[principle of contradiction]] itself, from which all proofs originally derive their certainty. At bottom this holds good of every geometrical theorem&nbsp;...}}

Although Schopenhauer could see no justification for trying to prove Euclid's parallel postulate, he did see a reason for examining another of Euclid's axioms.<ref>This comment by Schopenhauer was called "an acute observation" by [[T. L. Heath|Sir Thomas L. Heath]]. In his translation of [[Euclid's Elements|The Elements]], vol. 1, Book I, "Note on Common Notion 4", Heath made this judgment and also noted that Schopenhauer's remark "was a criticism in advance of [[Hermann von Helmholtz|Helmholtz']] theory". Helmholtz had "maintained that geometry requires us to assume the actual existence of rigid bodies and their free mobility in space" and is therefore "dependent on mechanics".</ref>

{{blockquote|text=It surprises me that the eighth axiom,<ref>What Schopenhauer calls the eighth axiom is Euclid's Common Notion 4.</ref> "Figures that coincide with one another are equal to one another", is not rather attacked. For ''"coinciding with one another"'' is either a mere [[Tautology (logic)|tautology]], or something quite [[empirical]], belonging not to pure intuition or perception, but to external sensuous experience. Thus it presupposes mobility of the figures, but [[matter]] alone is movable in [[space]]. Consequently, this reference to coincidence with one another forsakes pure space, the sole element of [[geometry]], in order to pass over to the material and empirical.<ref name="ReferenceB"/>}}

This follows Kant's reasoning.<ref>"Motion of an ''object'' in space does not belong in a pure science, and consequently not in geometry. For the fact that something is movable cannot be cognized ''a priori'', but can be cognized only through experience." (Kant, ''[[Critique of Pure Reason]]'', B 155, Note)</ref>

=== Ethics ===
{{Main|On the Basis of Morality}}

Schopenhauer asserts that the task of ethics is not to prescribe moral actions that ought to be done, but to investigate moral actions. As such, he states that philosophy is always theoretical: its task to explain what is given.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|at=Vol. 1, § 53.}}</ref>

According to Kant's transcendental idealism, space and time are forms of our sensibility in which phenomena appear in multiplicity. Reality [[thing-in-itself|in itself]] is free from multiplicity, not in the sense that an object is one, but that it is outside the ''possibility'' of multiplicity. Two individuals, though they appear distinct, are in-themselves not distinct.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|at=Vol. 1, § 23.}}</ref>

Appearances are entirely subordinated to the [[principle of sufficient reason]]. The egoistic individual who focuses his aims on his own interests has to deal with empirical laws as well as he can.

What is relevant for ethics are individuals who can act against their own self-interest. If we take a man who suffers when he sees his fellow men living in poverty and consequently uses a significant part of his income to support ''their'' needs instead of his ''own'' pleasures, then the simplest way to describe this is that he makes ''less distinction between himself'' and others than is usually made.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|at=Vol. 1, § 66.}}</ref>

Regarding how things ''appear'' to us, the egoist asserts a gap between two individuals, but the altruist experiences the sufferings of others as his own. In the same way a compassionate man cannot hurt animals, though they appear as distinct from himself.

What motivates the altruist is compassion. The suffering of others is for him not a cold matter to which he is indifferent, but he feels connectiveness to all beings. Compassion is thus the basis of morality.<ref>{{Cite book|title=On the Basis of Morality|last=Schopenhauer|first=Arthur|at=§ 19}}</ref>

==== Eternal justice ====
Schopenhauer calls the principle through which multiplicity appears the ''[[principium individuationis]]''. When we behold nature we see that it is a cruel battle for existence. Individual manifestations of the will can maintain themselves only at the expense of others—the will, as the only thing that exists, has no other option but to devour itself to experience pleasure. This is a fundamental characteristic of the will, and cannot be circumvented.<ref>{{Cite book|title=Parerga and Paralipomena|last=Schopenhauer|first=Arthur|at=Vol. 2, § 173}}</ref>

Unlike temporal or human justice, which requires time to repay an evil deed and "has its seat in the state, as requiting and punishing",<ref name="World as will and idea Vol. 1 § 63"/> eternal justice "rules not the state but the world, is not dependent upon human institutions, is not subject to chance and deception, is not uncertain, wavering, and erring, but infallible, fixed, and sure".<ref name="World as will and idea Vol. 1 § 63">''The World as Will and Idea'' Vol. 1 § 63</ref> Eternal justice is not retributive, because retribution requires time. There are no delays or reprieves. Instead, punishment is tied to the offence, "to the point where the two become one. ... Tormenter and tormented are one. The [Tormenter] errs in that he believes he is not a partaker in the suffering; the [tormented], in that he believes he is not a partaker in the guilt."<ref name="World as will and idea Vol. 1 § 63"/>

Suffering is the moral outcome of our attachment to pleasure. Schopenhauer deemed that this truth was expressed by the Christian dogma of [[original sin]] and, in Eastern religions, by the [[Reincarnation|dogma of rebirth.]]

==== Quietism ====
He who sees through the ''principium individuationis'' and comprehends suffering ''in general'' as his own will see suffering everywhere and, instead of fighting for the happiness of his individual manifestation, will abhor life itself since he knows that it is inseparably connected with suffering. For him, a happy individual life in a world of suffering is like a beggar who dreams one night that he is a king.<ref name="Ascetic">{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|at=Vol. 1, § 68.}}</ref>

Those who have experienced this intuitive knowledge cannot affirm life, but exhibit asceticism and quietism, meaning that they are no longer sensitive to motives, are not concerned about their individual welfare, and accept without resistance the evil that others inflict on them. They welcome poverty and neither seek nor flee death.<ref name="Ascetic"/> Schopenhauer referred to asceticism as the denial of the [[will to live]].

Human life is a ceaseless struggle for satisfaction and, instead of continuing their struggle, ascetics break it. It does not matter if these ascetics adhere to the dogmata of Christianity or to [[Dharmic faith|Dharmic religions]], since their way of living is the result of intuitive knowledge.

{{Blockquote|The [[Christian mysticism|Christian mystic]] and the teacher of the [[Vedanta philosophy]] agree in this respect also, they both regard all outward works and religious exercises as superfluous for him who has attained to perfection. So much agreement in the case of such different ages and nations is a practical proof that what is expressed here is not, as optimistic dullness likes to assert, an eccentricity and perversity of the mind, but an essential side of human nature, which only appears so rarely because of its excellence.<ref name="Ascetic"/>}}

==== Antinatalism ====

There is a debate whether Schopenhauer can be considered an early forerunner of [[antinatalism]] — the position that we have a duty not to bring people into existence. Some scholars link Schopenhauer to antinatalism and even to [[David Benatar]] explicitly.<ref name="Janaway-2025" /> Schopenhauer makes claims that could support categorizing him as an antinatalist: he says that everyone would not agree to come into existence but would reply "no thank you very much",<ref name="Schopenhauer-2018" />{{rp|page=595}}<ref name="Janaway-2025" /> that "life is a business that does not cover its costs",<ref name="Schopenhauer-2018" />{{rp|page=595}}<ref name="Janaway-2025" /> and that if people were rational they would not have children in his poignant rhetorical question:<ref name="Benatar-2006" />{{rp|page=163}}<ref name="Janaway-2025" />

{{blockquote
| text      = One should try to imagine that the act of procreation were neither a need, nor accompanied by sexual pleasure, but instead a matter of pure, rational reﬂection; could the human race even continue to exist? Would not everyone, on the contrary, have so much compassion for the coming generation that he would rather spare it the burden of existence, or at least refuse to take it upon himself to cold-bloodedly impose it on them?<ref name="Schopenhauer-2015" />{{rp|page=270}}
}}

On the other hand, one can support the opposite claim, that Schopenhauer rejects the antinatalist conclusion. His worldview sees individual human beings as mere appearances, and who do not come into existence upon birth.<ref name="Schopenhauer-2018" />{{rp|pages=480-481}}<ref name="Janaway-2025" /> Similarly, dying also does not lead to annihilation.<ref name="Schopenhauer-2018" />{{rp|pages=524}}<ref name="Janaway-2025" /> So, on a fundamental metaphysical level, it's impossible for us to never exist.<ref name="Janaway-2025" />

The way to reconcile the two positions is to put importance of coming into existence as a specific individual, who leads life full of suffering, instead of focusing on the metaphysical essence. This, however, does not decide the matter as Schopenhauer explicitly rejects any unconditional moral "oughts", which are necessary to convey the antinatalist position that we have a duty not to bring people into existence.<ref name="Janaway-2025" />

Furthermore, the new future child, Schopenhauer explains, already strives to exist in the world of appearances. And from the perspective of the parents, the moral judgment of an action depends on the mode of willing: if the motive is not malicious of selfish, then the action cannot be morally wrong.<ref name="Janaway-2025" />

But in the most important sense, preventing someone from existing as a person prevents them from negating his essence, which is will. Will can only be abolished through a person by getting to know the essence of the world. And this is the only real everlasting redemption. This is also the reason why Schopenhauer rejects suicide as the solution to the condition of life.<ref name="Janaway-2025" />

So, Schopenhauer presents reasons for two opposing views, each having merit from a different perspective. From the perspective of the individual, it would be good not to bring him into existence to spare him suffering. But from the more fundamental perspective, it's good to allow them to come into being so they can grasp the fundamental truth of the world and attain redemption through annealing the will to life. In this way, he shares the sentiment of antinatalists in their basis but does not follow them to their conclusion.<ref name="Janaway-2025" />

=== Psychology ===
Philosophers have not traditionally been impressed by the necessity of sex, but Schopenhauer addressed sex and related concepts forthrightly:

{{blockquote|...&nbsp;one ought rather to be surprised that a thing [sex] which plays throughout so important a part in human life has hitherto practically been disregarded by philosophers altogether, and lies before us as raw and untreated material.<ref>Schopenhauer, Arthur. [[:s:The World as Will and Representation/Supplements to the Fourth Book|''The World as Will and Representation: Supplements to the Fourth Book'']]</ref>}}

He named a force within man that he felt took invariable precedence over reason: the [[will to live]] or will to life (''Wille zum Leben''), defined as an inherent drive within human beings, and all creatures, to stay alive; a force that inveigles<ref name="inveigles">{{cite book|title=The Oxford Encyclopedic English Dictionary|year=1991|publisher=Oxford University Press|location=Schopenhauer|isbn=978-0-19-861248-3|page=1298}}</ref> us into reproducing.

Schopenhauer refused to conceive of love as either trifling or accidental, but rather understood it as an immensely powerful force that lay unseen within man's [[psyche (psychology)|psyche]], guaranteeing the quality of the human race:

{{blockquote|The ultimate aim of all love affairs ... is more important than all other aims in man's life; and therefore it is quite worthy of the profound seriousness with which everyone pursues it. What is decided by it is nothing less than the composition of the next generation ...<ref>Schopenhauer, Arthur, [[:s:The World as Will and Representation/Supplements to the Fourth Book|''The World as Will and Representation'', Supplements to the Fourth Book]]</ref>}}

It has often been argued that Schopenhauer's thoughts on sexuality foreshadowed the [[evolution|theory of evolution]], a claim met with satisfaction by [[Charles Darwin|Darwin]] as he included a quotation from Schopenhauer in his ''[[The Descent of Man, and Selection in Relation to Sex|Descent of Man]]''.<ref>{{Cite book|url=https://en.wikisource.org/wiki/Page%3ADescent_of_Man_1875.djvu/602|title=The Descent of Man|last=Darwin|first=Charles|page=586|archive-date=21 October 2021|access-date=3 December 2017|archive-url=https://web.archive.org/web/20211021102750/https://en.wikisource.org/wiki/Page%3ADescent_of_Man_1875.djvu/602|url-status=live}}</ref> This has also been noted about [[Sigmund Freud|Freud]]'s concepts of the [[libido]] and the [[unconscious mind]], and [[evolutionary psychology]] in general.<ref>"Nearly a century before Freud ... in Schopenhauer there is, for the first time, an explicit philosophy of the unconscious and of the body." Safranski p. 345.</ref>

=== Political and social thought ===

==== Politics ====
[[File:FFM Wallanlagen Schopenhauer-Denkmal.jpg|thumb|Bust in [[Frankfurt]]]]
Schopenhauer's politics were an echo of his system of ethics, which he elucidated in detail in his ''Die beiden Grundprobleme der Ethik'' (the two essays ''On the Freedom of the Will'' and ''On the Basis of Morality'').

In occasional political comments in his ''[[Parerga and Paralipomena]]'' and ''Manuscript Remains'', Schopenhauer described himself as a proponent of [[limited government]]. Schopenhauer shared the view of [[Thomas Hobbes]] on the necessity of the state and state action to check the innate destructive tendencies of our species. He also defended the independence of the legislative, judicial and executive branches of power, and a monarch as an impartial element able to practise justice (in a practical and everyday sense, not a cosmological one).<ref>''[[The World as Will and Representation]]'', Vol. 2, Ch. 47</ref>

He declared that monarchy is "natural to man in almost the same way as it is to bees and ants, to cranes in flight, to wandering elephants, to wolves in a pack in search of prey, and to other animals".<ref name="Paralipomena, Vol p. 254">''Parerga and Paralipomena, Vol. 2'', "On Jurisprudence and Politics," §127, trans. Payne (p. 254).</ref> Intellect in monarchies, he writes, always has "much better chances against stupidity, its implacable and ever-present foe, than it has in republics; but this is a great advantage."<ref name="Paralipomena, Vol p. 254"/> On the other hand, Schopenhauer disparaged [[republicanism]] as being "as unnatural to man as it is unfavorable to higher intellectual life and thus to the arts and sciences".<ref>''Parerga and Paralipomena, Vol. 2'', "On Jurisprudence and Politics," §127, trans. Payne (p. 255).</ref>

By his own admission, Schopenhauer did not give much thought to politics, and several times he wrote proudly of how little attention he paid "to political affairs of [his] day". In a life that spanned several revolutions in French and German government, and a few continent-shaking wars, he maintained his position of "minding not the times but the eternities". He wrote many disparaging remarks about Germany and the Germans. A typical example is: "For a German it is even good to have somewhat lengthy words in his mouth, for he thinks slowly, and they give him time to reflect."<ref>''[[The World as Will and Representation]]'', Vol. 2, Ch. 12</ref>

==== Punishment ====
The State, Schopenhauer claimed, punishes criminals to prevent future crimes. It places "beside every possible motive for committing a wrong a more powerful motive for leaving it undone, in the inescapable punishment. Accordingly, the criminal code is as complete a register as possible of counter-motives to all criminal actions that can possibly be imagined&nbsp;..."<ref name="twwr62">Schopenhauer, ''[[The World as Will and Representation]]'', Vol. I, § 62.</ref> He claimed that this doctrine was not original to him but had appeared in the writings of [[Plato]],<ref>"...&nbsp;he who attempts to punish in accordance with reason does not retaliate on account of the past wrong (for he could not undo something which has been done) but for the future, so that neither the wrongdoer himself, nor others who see him being punished, will do wrong again." Plato, "[[Protagoras (dialogue)|Protagoras]]", 324 B. Plato wrote that punishment should "be an example to other men not to offend". Plato, "[[Laws (dialogue)|Laws]]", Book IX, 863.</ref> [[Seneca the Younger]], [[Thomas Hobbes]], [[Samuel von Pufendorf]] and [[Paul Johann Anselm Ritter von Feuerbach]].

==== Races and religions ====
Schopenhauer attributed civilizational primacy to the northern "white races" due to their sensitivity and creativity (except for the ancient Egyptians and Hindus, whom he saw as equal):

<blockquote>The highest civilization and culture, apart from the [[History of Hinduism|ancient Hindus]] and [[Ancient Egypt|Egyptians]], are found exclusively among the white races; and even with many dark peoples, the ruling caste or race is fairer in colour than the rest and has, therefore, evidently immigrated, for example, the [[Brahmans]], the [[Inca Empire|Incas]], and the rulers of the [[South Sea Islands]]. All this is due to the fact that necessity is the mother of invention because those tribes that emigrated early to the north, and there gradually became white, had to develop all their intellectual powers and invent and perfect all the arts in their struggle with need, want and misery, which in their many forms were brought about by the climate. This they had to do in order to make up for the parsimony of nature and out of it all came their high civilization.<ref>''Parerga and Paralipomena'', Vol. 2, "On Philosophy and Natural Science," §92, trans. Payne (p. 158-159).</ref></blockquote>

Schopenhauer was fervently [[abolitionism|opposed to slavery]]. Speaking of the treatment of slaves in the [[Slavery in the United States|slave-holding states of the United States]], he condemned "those devils in human form, those bigoted, church-going, strict sabbath-observing scoundrels, especially the Anglican parsons among them" for how they "treat their innocent black brothers who through violence and injustice have fallen into their devil's claws". The slave-holding states of North America, Schopenhauer writes, are a "disgrace to the whole of humanity".<ref>''Parerga and Paralipomena'', Vol. 2, "On Ethics," §114, trans. Payne (p. 212).</ref>

Schopenhauer also maintained a marked metaphysical and political [[anti-Judaism]]. He argued that Christianity constituted a revolt against what he styled the materialistic basis of Judaism, exhibiting an Indian-influenced ethics reflecting the [[Aryan]]-[[Vedas|Vedic]] theme of spiritual self-conquest. He saw this as opposed to the ignorant drive toward earthly utopianism and superficiality of a worldly "Jewish" spirit:

<blockquote>[Judaism] is, therefore, the crudest and poorest of all religions and consists merely in an absurd and revolting [[theism]]. It amounts to this that the [[Kyrios|''κύριος'' ['Lord']]], who has created the world, desires to be worshipped and adored; and so above all he is jealous, is envious of his colleagues, of all the other gods; if sacrifices are made to them he is furious and his Jews have a bad time ... It is most deplorable that this religion has become the basis of the prevailing religion of Europe; for it is a religion without any metaphysical tendency. While all other religions endeavor to explain to the people by symbols the metaphysical significance of life, the religion of the Jews is entirely immanent and furnishes nothing but a mere war-cry in the struggle with other nations.<ref>"Fragments for the History of Philosophy", ''Parerga and Paralipomena'', Volume I, trans. Payne (p. 126).</ref></blockquote>

==== Women ====
In his 1851 essay "On Women", Schopenhauer expressed opposition to what he called "Teutonico-Christian stupidity" of "reflexive, unexamined reverence for the female (''abgeschmackten Weiberveneration'')".<ref>{{Cite web|url=https://aboq.org/schopenhauer/parerga2/weiber.htm|title=Arthur Schopenhauer: Ueber die Weiber|website=aboq.org|access-date=19 September 2022|archive-date=18 October 2023|archive-url=https://web.archive.org/web/20231018015127/https://aboq.org/schopenhauer/parerga2/weiber.htm|url-status=live}}</ref> He wrote: "Women are directly fitted for acting as the nurses and teachers of our early childhood by the fact that they are themselves childish, frivolous and short-sighted; in a word, they are big children all their life long—a kind of intermediate stage between the child and the full-grown man." He opined that women are deficient in artistic faculties and sense of justice, and expressed his opposition to [[monogamy]].<ref>[[Nigel Rodgers|Rodgers]] (environmentalist) and [[Mel Thompson (writer)|Thompson]] in ''[[Philosophers Behaving Badly]]'' call Schopenhauer "a misogynist without rival in ... Western philosophy".</ref> He claimed that "woman is by nature meant to obey". The essay does give some compliments: "women are decidedly more sober in their judgment than [men] are", and are more sympathetic to the suffering of others.

Schopenhauer's writings influenced many, from [[Friedrich Nietzsche]] to nineteenth-century [[feminists]],<ref>''Feminism and the Limits of Equality'' PA Cain – Ga. L. Rev., 1989</ref> and continue to inspire [[Sexism|sexist]] views today. His [[biology|biological]] analysis of the difference between the sexes, and their separate roles in the struggle for survival and reproduction, anticipates some of the claims that were later ventured by [[sociobiology|sociobiologists]] and [[evolutionary psychology|evolutionary psychologists]].<ref name="Young2005">{{cite book|author=Julian Young|title=Schopenhauer|url=https://books.google.com/books?id=gfDyeGY0RFMC&pg=PA242|date=23 June 2005|publisher=Psychology Press|isbn=978-0-415-33346-7|page=242}}</ref>

When the elderly Schopenhauer sat for [[Arthur Schopenhauer (sculpture)|a sculpture portrait]] by the Prussian sculptor [[Elisabet Ney]] in 1859, he was much impressed by the young woman's wit and independence, as well as by her skill as a visual artist.<ref>{{Cite journal|title=Arthur Schopenhauer and Elisabet Ney|first=Sandra Salser|last=Long|journal=[[Southwest Review]]|volume=69|number=2|date=Spring 1984|pages=130–47|jstor=43469632}}</ref> After his time with Ney, he told Richard Wagner's friend [[Malwida von Meysenbug]]: "I have not yet spoken my last word about women. I believe that if a woman succeeds in withdrawing from the mass, or rather raising herself above the mass, she grows ceaselessly and more than a man."<ref>Safranski (1990), Chapter 24. p. 348.</ref>

==== Pederasty ====
In the third, expanded edition of ''The World as Will and Representation'' (1859), Schopenhauer added an appendix to his chapter on the ''Metaphysics of Sexual Love''. He wrote that [[pederasty]] has the benefit of preventing ill-begotten children. Concerning this, he stated that "the vice we are considering appears to work directly against the aims and ends of nature, and that in a matter that is all important and of the greatest concern to her it must in fact serve these very aims, although only indirectly, as a means for preventing greater evils."<ref>{{Harvard citation no brackets|Schopenhauer|1969|p=566}}</ref>
Schopenhauer ends the appendix with the statement that "by expounding these paradoxical ideas, I wanted to grant to the professors of philosophy a small favour. I have done so by giving them the opportunity of slandering me by saying that I defend and commend pederasty."<ref>{{Harvard citation no brackets|Schopenhauer|1969|p=567}}</ref>

==== Heredity and eugenics ====
[[File:Frankfurt Am Main-Portraits-Arthur Schopenhauer-1845 (cropped).jpg|thumb|upright|Schopenhauer at age 58 on 16 May 1846]]

Schopenhauer viewed personality and [[intellect]] as inherited. He quotes [[Horace]]'s saying, "From the brave and good are the brave descended" (''Odes'', iv, 4, 29) and Shakespeare's line from ''[[Cymbeline]]'', "Cowards father cowards, and base things sire base" (IV, 2) to reinforce his hereditarian argument.<ref>Payne, ''The World as Will and Representation'', Vol. II, p. 519</ref> Mechanistically, Schopenhauer believed that a person inherits his intellect through his mother, and personal character through the father.<ref>''On the Suffering of the World'' (1970), p. 35. Penguin Books – Great Ideas.</ref> This belief in heritability of traits informed Schopenhauer's view of love—placing it at the highest level of importance. For Schopenhauer the "final aim of all love intrigues, be they comic or tragic, is really of more importance than all other ends in human life. What it all turns upon is nothing less than the composition of the next generation. ... It is not the weal or woe of any one individual, but that of the human race to come, which is here at stake." This view of the importance for the species of whom we choose to love was reflected in his views on [[eugenics]] or good breeding. Here Schopenhauer wrote:

<blockquote>With our knowledge of the complete unalterability both of character and of mental faculties, we are led to the view that a real and thorough improvement of the human race might be reached not so much from outside as from within, not so much by theory and instruction as rather by the path of generation. Plato had something of the kind in mind when, in the fifth book of his ''Republic'', he explained his plan for increasing and improving his warrior caste. If we could castrate all scoundrels and stick all stupid geese in a convent, and give men of noble character a whole [[harem]], and procure men, and indeed thorough men, for all girls of intellect and understanding, then a generation would soon arise which would produce a better age than that of [[Pericles]].<ref>{{cite book | last = Schopenhauer | first = Arthur | title = The World as Will and Representation |editor=E. F. J. Payne |volume=II | publisher = Dover Publications | location = New York | year = 1969 |isbn=978-0-486-21762-8 |page=527 }}</ref></blockquote>

In another context, Schopenhauer reiterated his eugenic thesis: "If you want Utopian plans, I would say: the only solution to the problem is the [[despotism]] of the wise and noble members of a genuine aristocracy, a genuine nobility, achieved by mating the most magnanimous men with the cleverest and most gifted women. This proposal constitutes my Utopia and my Platonic Republic."<ref>''Essays and Aphorisms'', trans. R.J. Hollingdale, Middlesex: London, 1970, p. 154</ref> Analysts (e.g., [[Keith Ansell-Pearson]]) have suggested that Schopenhauer's anti-[[egalitarianism|egalitarianist]] sentiment and his support for eugenics influenced the neo-aristocratic philosophy of Friedrich Nietzsche, who initially considered Schopenhauer his mentor.<ref>''Nietzsche and Modern German Thought'' by K. Ansell-Pearson – 1991 – Psychology Press.</ref>

==== Animal rights ====
{{Main|Arthur Schopenhauer's view on animal rights}}
As a consequence of his [[Monism|monistic]] philosophy, Schopenhauer was very concerned about animal welfare and rights.<ref>Christina Gerhardt, "Thinking With: Animals in Schopenhauer, Horkheimer and Adorno." ''Critical Theory and Animals''. Ed. John Sanbonmatsu. Lanham: Rowland, 2011. 137–157.</ref><ref name="Puryear">Stephen Puryear, [https://philpapers.org/rec/PURSOT "Schopenhauer on the Rights of Animals." ''European Journal of Philosophy'' 25/2 (2017):250–269] {{Webarchive|url=https://web.archive.org/web/20230409102359/https://philpapers.org/rec/PURSOT |date=9 April 2023 }}.</ref> For him, all individual animals, including humans, are essentially phenomenal manifestations of the one underlying Will. For him the word "will" designates force, power, impulse, energy, and desire; it is the closest word we have that can signify both the essence of all external things and our own direct, inner experience. Since every living thing possesses will, humans and animals are fundamentally the same and can recognize themselves in each other.<ref>"Unlike the intellect, it [the Will] does not depend on the perfection of the organism, but is essentially the same in all animals as what is known to us so intimately. Accordingly, the animal has all the emotions of humans, such as joy, grief, fear, anger, love, hatred, strong desire, envy, and so on. The great difference between human and animal rests solely on the intellect's degrees of perfection. ''On the Will in Nature'', "Physiology and Pathology".</ref> For this reason, he claimed that a good person would have sympathy for animals, who are our fellow sufferers.

{{blockquote|Compassion for animals is intimately associated with goodness of character, and it may be confidently asserted that he who is cruel to living creatures cannot be a good man.|''[[On the Basis of Morality]]'', § 19}}

{{blockquote|Nothing leads more definitely to a recognition of the identity of the essential nature in animal and human phenomena than a study of zoology and anatomy.|''On the Basis of Morality'', chapter 8<ref>Quoted in {{cite book | last = Schopenhauer | first = Arthur | title = Philosophical Writings | publisher = Continuum | location = London | year = 1994 |isbn=978-0-8264-0729-0 |page=233}}</ref>}}

{{blockquote|The assumption that animals are without rights and the illusion that our treatment of them has no moral significance is a positively outrageous example of Western crudity and barbarity. Universal compassion is the only guarantee of morality.|''On the Basis of Morality'', chapter 8<ref>Quoted in {{cite book | last = Ryder | first = Richard | title = Animal Revolution: Changing Attitudes Towards Speciesism | publisher = Berg Publishers | location = Oxford | year = 2000 |isbn=978-1-85973-330-1 |page=57}}</ref>}}

In 1841 he praised the establishment in London of the [[Society for the Prevention of Cruelty to Animals]], and in Philadelphia of the Animals' Friends Society. Schopenhauer went so far as to protest using the pronoun "it" in reference to animals because that led to treatment of them as though they were inanimate things.<ref>"... in English all animals are of the neuter gender and so are represented by the pronoun 'it,' just as if they were inanimate things. The effect of this artifice is quite revolting, especially in the case of primates, such as dogs, monkeys, and the like...." ''On the Basis of Morality'', § 19.</ref> To reinforce his points, Schopenhauer referred to anecdotal reports of the look in the eyes of a monkey who had been shot<ref>"I recall having read of an Englishman who, while hunting in India, had shot a monkey; he could not forget the look which the dying animal gave him, and since then had never again fired at monkeys." ''On the Basis of Morality'', § 19.</ref> and also the grief of a baby elephant whose mother had been killed by a hunter.<ref>"[Sir William Harris] describes how he shot his first elephant, a female. The next morning he went to look for the dead animal; all the other elephants had fled from the neighborhood except a young one, who had spent the night with its dead mother. Forgetting all fear, he came toward the sportsmen with the clearest and liveliest evidence of inconsolable grief, and put his tiny trunk round them in order to appeal to them for help. Harris says he was then filled with real remorse for what he had done, and felt as if he had committed a murder." ''On the basis of morality'', § 19.</ref>

Schopenhauer was very attached to his succession of pet poodles. He criticized [[Baruch Spinoza]]'s<ref>"His contempt for animals, who, as mere things for our use, are declared by him to be without rights, ... in conjunction with Pantheism, is at the same time absurd and abominable." ''The World as Will and Representation'', Vol. 2, Chapter 50.</ref> belief that animals are a mere means for the satisfaction of humans.<ref>Spinoza, ''Ethics'', Pt. IV, Prop. XXXVII, Note I.: "Still I do not deny that beasts feel: what I deny is, that we may not consult our own advantage and use them as we please, treating them in a way which best suits us; for their nature is not like ours&nbsp;..." This is the exact opposite of Schopenhauer's doctrine. Also, ''Ethics'', Appendix, 26, "whatsoever there be in nature beside man, a regard for our advantage does not call on us to preserve, but to preserve or destroy according to its various capacities, and to adapt to our use as best we may."</ref><ref>"Such are the matters which I engage to prove in Prop. xviii of this Part, whereby it is plain that the law against the slaughtering of animals is founded rather on vain superstition and womanish pity than on sound reason. The rational quest of what is useful to us further teaches us the necessity of associating ourselves with our fellow-men, but not with beasts, or things, whose nature is different from our own; we have the same rights in respect to them as they have in respect to us. Nay, as everyone's right is defined by his virtue, or power, men have far greater rights over beasts than beasts have over men. Still I affirm that beasts feel. But I also affirm that we may consult our own advantage and use them as we please, treating them in the way which best suits us; for their nature is not like ours, and their emotions are naturally different from human emotions." ''Ethics'', Part 4, Prop. 37, Note 1.</ref> Tim Madigan wrote that despite all of his bombast, Schopenhauer was a sympathetic character who had concerns for the suffering of animals.
{{blockquote|The greatest benefit conferred by the railways is that they spare millions of draught-horses their miserable existences.|''Essays and Aphorisms'', p. 171<ref>Quoted in {{Cite web|last=Madigan|first=Tim|title=Schopenhauer's Compassionate Morality {{!}} Issue 52 {{!}} Philosophy Now|url=https://philosophynow.org/issues/52/Schopenhauers_Compassionate_Morality#:~:text=It%20is%20compassion,%20or%20fellow,of%20the%20will%20to%20live.|website=Philosophy Now|access-date=2023-09-16|archive-date=18 September 2023|archive-url=https://web.archive.org/web/20230918004248/https://philosophynow.org/issues/52/Schopenhauers_Compassionate_Morality#:~:text=It%20is%20compassion,%20or%20fellow,of%20the%20will%20to%20live.|url-status=live}}</ref>}}

=== Intellectual interests and affinities ===
Schopenhauer had a wide range of interests, from science and opera to occultism and literature.

In his student years, Schopenhauer went more often to lectures in the sciences than philosophy. He kept a strong interest as his personal library contained near to 200 books of scientific literature at his death, and his works refer to scientific titles not found in the library.{{r|Cartwright-2010|p=170}}

Many evenings were spent in the theatre, opera and ballet; Schopenhauer especially liked the operas of [[Wolfgang Amadeus Mozart]], [[Gioachino Rossini]] and [[Vincenzo Bellini]].<ref>{{Cite book |last=Carnegy |first=Patrick |title=Wagner and the Art of the Theatre |page=51}}</ref> Schopenhauer considered music the highest art, and played the flute during his whole life.{{r|Cartwright-2010|p=30}}

As a polyglot, he knew German, Italian, Spanish, French, English, [[Latin]] and [[ancient Greek]], and was an avid reader of poetry and literature. He particularly revered Goethe, [[Petrarch]], [[Pedro Calderón de la Barca]] and [[William Shakespeare]].

<blockquote>If Goethe had not been sent into the world simultaneously with Kant in order to counterbalance him, so to speak, in the spirit of the age, the latter would have been haunted like a nightmare many an aspiring mind and would have oppressed it with great affliction. But now the two have an infinitely wholesome effect from opposite directions and will probably raise the German spirit to a height surpassing even that of antiquity.{{r|Cartwright-2010|p=240}}</blockquote>

In philosophy, his most important influences were, according to himself, Kant, Plato and the [[Upanishads]].

==== Indology ====
[[File:Schopenhauer 1852.jpg|thumb|Schopenhauer, 1852]]

Schopenhauer read the Latin translation of the [[Hindu texts|ancient Hindu texts]], the ''[[Upanishads]]'', translated by the French writer [[Abraham Hyacinthe Anquetil-Duperron|Anquetil du Perron]]{{sfn|Clarke|1997|page=68}} from the Persian translation of Prince [[Dara Shukoh]] entitled ''Sirre-Akbar'' ("The Great Secret"). He was so impressed by its [[Indian philosophy|philosophy]] that he called it "the production of the highest human wisdom", and believed it contained superhuman concepts. Schopenhauer considered India as "the land of the most ancient and most pristine wisdom, the place from which [[Europeans]] could trace their descent and the tradition by which they had been influenced in so many decisive ways",{{sfn|Clarke|1997|page=68}} and regarded the ''Upanishads'' as "the most profitable and elevating reading which [...] is possible in the world. It has been the solace of my life, and will be the solace of my death."{{sfn|Clarke|1997|page=68}} In ''The World as Will and Representation'', he writes:

<blockquote>If the reader has also received the benefit of the Vedas, the access to which by means of the Upanishads is in my eyes the greatest privilege which this still young century (1818) may claim before all previous centuries, if then the reader, I say, has received his initiation in primeval Indian wisdom, and received it with an open heart, he will be prepared in the very best way for hearing what I have to tell him. It will not sound to him strange, as to many others, much less disagreeable; for I might, if it did not sound conceited, contend that every one of the detached statements which constitute the Upanishads, may be deduced as a necessary result from the fundamental thoughts which I have to enunciate, though those deductions themselves are by no means to be found there.<ref>''The World as Will and Representation'' Preface to the first edition, p. xiii</ref></blockquote>

Schopenhauer was first introduced to Anquetil du Perron's translation by Friedrich Majer in 1814.{{sfn|Clarke|1997|page=68}} They met during the winter of 1813–1814 in [[Weimar]] at the home of Schopenhauer's mother, according to the biographer Safranski. Majer was a follower of [[Johann Gottfried Herder|Herder]], and an early [[Indologist]]. Schopenhauer did not begin serious study of the Indic texts until the summer of 1814. Safranski maintains that, between 1815 and 1817, Schopenhauer had another important cross-pollination with Indian thought in [[Dresden]]. This was through his neighbor of two years, [[Karl Christian Friedrich Krause]]. Krause was then a minor and rather unorthodox philosopher who attempted to mix his own ideas with ancient Indian wisdom. Krause had also mastered [[Sanskrit]], unlike Schopenhauer, and they developed a professional relationship. It was from Krause that Schopenhauer learned [[meditation]] and received the closest thing to expert advice concerning Indian thought.<ref>Christopher McCoy, 3–4</ref>

{{blockquote|The view of things [...] that all plurality is only apparent, that in the endless series of individuals, passing simultaneously and successively into and out of life, generation after generation, age after age, there is but one and the same entity really existing, which is present and identical in all alike;—this theory, I say, was of course known long before Kant; indeed, it may be carried back to the remotest antiquity. It is the alpha and omega of the oldest book in the world, the sacred [[Vedas]], whose dogmatic part, or rather esoteric teaching, is found in the Upanishads. There, in almost every page this profound doctrine lies enshrined; with tireless repetition, in countless adaptations, by many varied parables and similes it is expounded and inculcated.|''On the Basis of Morality'', chapter 4<ref>{{cite book |last=Schopenhauer |first=Arthur |year=1840 |publication-date=1908 |title=[[On the Basis of Morality]] |chapter-url=https://archive.org/stream/basisofmorality00schoiala#page/269/mode/2up |chapter=Part IV |translator-last=Bullock |translator-first=Arthur Brodrick |location=London |publisher=[[Swan Sonnenschein]] |pages=269–271 |via=[[Internet Archive]]}}</ref>}}

For Schopenhauer, will had [[ontology|ontological]] primacy over the [[intellect]]; desire is prior to thought. Schopenhauer felt this was similar to notions of [[puruṣārtha]] or goals of life in [[Vedānta]] [[Hinduism]].

In Schopenhauer's philosophy, denial of the will is attained by:
* personal experience of an extremely great suffering that leads to loss of the [[will to live]]; or
* knowledge of the essential nature of life in the world through observation of the suffering of other people.

The book ''Oupnekhat'' (Upanishad) always lay open on his table, and he invariably studied it before going to bed. He called the opening up of [[Sanskrit literature]] "the greatest gift of our century", and predicted that the philosophy and knowledge of the Upanishads would become the cherished faith of the West.<ref>{{cite web|url=http://www.philosophy.ru/library/asiatica/indica/authors/motives.html|title=Western Indologists: A Study in Motives|last=Dutt|first=Purohit Bhagavan|access-date=9 May 2009|archive-url=https://web.archive.org/web/20100802010348/http://www.philosophy.ru/library/asiatica/indica/authors/motives.html|archive-date=2 August 2010}}</ref> Most noticeable, in the case of Schopenhauer's work, was the significance of the ''[[Chandogya Upanishad]]'', whose [[Mahāvākyas|Mahāvākya]], [[Tat Tvam Asi]], is mentioned throughout ''The World as Will and Representation''.<ref>Christopher McCoy, 54–56</ref>

==== Buddhism ====
Schopenhauer noted a correspondence between his doctrines and the [[Four Noble Truths]] of [[Buddhism]].<ref>Abelson, Peter (April 1993).
[http://ccbs.ntu.edu.tw/FULLTEXT/JR-PHIL/peter2.htm Schopenhauer and Buddhism] {{Webarchive|url=https://web.archive.org/web/20110628204330/http://ccbs.ntu.edu.tw/FULLTEXT/JR-PHIL/peter2.htm |date=28 June 2011 }}. ''Philosophy East and West'' Volume 43, Number 2, pp. 255–278. University of Hawaii Press. Retrieved on: 12 April 2008.</ref> Similarities centered on the principles that life involves suffering, that suffering is caused by desire ([[taṇhā]]), and that the extinction of desire leads to liberation. Thus three of the four "truths of the Buddha" correspond to Schopenhauer's doctrine of the will.<ref>[[Christopher Janaway|Janaway]], Christopher, ''Self and World in Schopenhauer's Philosophy'', pp. 28&nbsp;ff.</ref> In Buddhism, while greed and lust are always unskillful, desire is ethically variable – it can be skillful, unskillful, or neutral.<ref name="David Burton 2004, page 22">David Burton, "Buddhism, Knowledge and Liberation: A Philosophical Study." Ashgate Publishing, Ltd., 2004, p. 22.</ref>

Buddhist [[nirvāṇa]] is not equivalent to the condition that Schopenhauer described as denial of the will. Nirvāṇa is not the extinguishing of the ''person'' as some Western scholars have thought, but only the "extinguishing" (the literal meaning of nirvana) of the flames of greed, hatred, and delusion that assail a person's character.<ref>John J. Holder, ''Early Buddhist Discourses.'' Hackett Publishing Company, 2006, p. xx.</ref> Schopenhauer made the following statement in his discussion of religions:<ref>
"Schopenhauer is often said to be the first modern Western philosopher to attempt integration of his work with Eastern ways of thinking. That he was the first is true, but the claim that he was ''influenced'' by Indian thought needs qualification. There is a remarkable correspondence in broad terms between some central Schopenhauerian doctrines and Buddhism: notably in the views that empirical existence is suffering, that suffering originates in desires, and that salvation can be attained by the extinction of desires. These three 'truths of the Buddha' are mirrored closely in the essential structure of the doctrine of the will." (On this, see Dorothea W. Dauer, ''Schopenhauer as Transmitter of Buddhist Ideas''. Note also the discussion by Bryan Magee, ''The Philosophy of Schopenhauer'', pp. 14–15, 316–321). Janaway, Christopher, ''Self and World in Schopenhauer's Philosophy'', p. 28&nbsp;f.
</ref>

<blockquote>If I wished to take the results of my philosophy as the standard of truth, I should have to concede to Buddhism pre-eminence over the others. In any case, it must be a pleasure to me to see my doctrine in such close agreement with a religion that the majority of men on earth hold as their own, for this numbers far more followers than any other. And this agreement must be yet the more pleasing to me, inasmuch as ''in my philosophizing I have certainly not been under its influence'' [emphasis added]. For up till 1818, when my work appeared, there was to be found in Europe only a very few accounts of Buddhism.<ref>''[[The World as Will and Representation]]'', Vol. 2, Ch. 17</ref></blockquote>

Buddhist philosopher [[Keiji Nishitani]] sought to distance Buddhism from Schopenhauer.<ref>''Artistic detachment in Japan and the West: psychic distance in comparative aesthetics'' by S. Odin – 2001 – University of Hawaii Press.</ref> While Schopenhauer's philosophy may sound rather mystical in such a summary, his [[methodology]] was resolutely [[empirical]], rather than speculative or transcendental:

<blockquote>Philosophy ... is a science, and as such has no articles of faith; accordingly, in it nothing can be assumed as existing except what is either positively given empirically, or demonstrated through indubitable conclusions.<ref>''Parerga & Paralipomena'', vol. I, p. 106., trans. E.F.J. Payne.</ref></blockquote>

Also note:

<blockquote>This actual world of what is knowable, in which we are and which is in us, remains both the material and the limit of our consideration.<ref>''World as Will and Representation'', vol. I, p. 273, trans. E.F.J. Payne.</ref></blockquote>

The argument that Buddhism affected Schopenhauer's philosophy more than any other [[Dharma|Dharmic]] faith loses credence since he did not begin a serious study of Buddhism until after the publication of ''The World as Will and Representation'' in 1818.<ref>Christopher McCoy, 3</ref> Scholars have started to revise earlier views about Schopenhauer's discovery of Buddhism. Proof of early interest and influence appears in Schopenhauer's 1815–16 notes (transcribed and translated by Urs App) about Buddhism. They are included in a recent case study that traces Schopenhauer's interest in Buddhism and documents its influence.<ref>App, Urs [http://www.sino-platonic.org/complete/spp200_schopenhauer.pdf Arthur Schopenhauer and China. ''Sino-Platonic Papers'' Nr. 200 (April 2010)] {{Webarchive|url=https://web.archive.org/web/20100704192558/http://www.sino-platonic.org/complete/spp200_schopenhauer.pdf |date=4 July 2010 }} (PDF, 8.7&nbsp;Mb PDF, 164 p.; Schopenhauer's early notes on Buddhism reproduced in Appendix). This study provides an overview of the actual discovery of Buddhism by Schopenhauer.</ref> Other scholarly work questions how similar Schopenhauer's philosophy actually is to Buddhism.<ref>Hutton, Kenneth [http://blogs.dickinson.edu/buddhistethics/files/2014/12/Hutton-Schopenhauer.pdf Compassion in Schopenhauer and Śāntideva. ''Journal of Buddhist Ethics'' Vol. 21 (2014)] {{Webarchive|url=https://web.archive.org/web/20150414055301/http://blogs.dickinson.edu/buddhistethics/files/2014/12/Hutton-Schopenhauer.pdf |date=14 April 2015 }}</ref>

==== Magic and occultism ====
Some traditions in [[Western esotericism]] and [[parapsychology]] interested Schopenhauer and influenced his philosophical theories. He praised [[animal magnetism]] as evidence for the reality of magic in his ''On the Will in Nature'', and went so far as to accept the division of magic into [[Left-hand path and right-hand path|left-hand and right-hand magic]], although he doubted the existence of demons.<ref name="Myth of Disenchantment">{{Cite book | last = Josephson-Storm | first = Jason | title = The Myth of Disenchantment: Magic, Modernity, and the Birth of the Human Sciences | location = Chicago | publisher = University of Chicago Press | date = 2017 |pages = 187–188 | url = https://books.google.com/books?id=xZ5yDgAAQBAJ |isbn=978-0-226-40336-6 }}</ref>

Schopenhauer grounded magic in the Will and claimed all forms of magical transformation depended on the human Will, not on ritual. This theory notably parallels [[Aleister Crowley]]'s system of magic and its emphasis on human will.<ref name="Myth of Disenchantment" /> Given the importance of the Will to Schopenhauer's overarching system, this amounts to "suggesting his whole philosophical system had magical powers."<ref>Quote from Josephson-Storm (2017), p. 188.</ref> Schopenhauer rejected the theory of [[disenchantment]] and claimed philosophy should synthesize itself with magic, which he believed amount to "practical metaphysics".<ref>Josephson-Storm (2017), pp. 188–189.</ref>

[[Neoplatonism]], including the traditions of [[Plotinus]] and to a lesser extent [[Marsilio Ficino]], has also been cited as an influence on Schopenhauer.<ref>{{cite book|last=Anderson |first=Mark |title=Pure: Modernity, Philosophy, and the One |chapter=Experimental Subversions of Modernity |date=2009 |publisher=Sophia Perennis |isbn=978-1-59731-094-9}}</ref>

== Thoughts on other philosophers ==

=== Giordano Bruno and Spinoza ===
Schopenhauer saw [[Giordano Bruno]] and Spinoza as philosophers not bound to their age or nation. "Both were fulfilled by the thought, that as manifold the appearances of the world may be, it is still ''one'' being, that appears in all of them. ... Consequently, there is no place for God as creator of the world in their philosophy, but God is the world itself."<ref name="Spinoza and Bruno">{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|at=Vol. 1, Criticism of the Kantian Philosophy. Note 5.}}</ref><ref name="Presentation">{{cite web|url=http://gutenberg.spiegel.de/buch/arthur-schopenhauers-handschriftlicher-nachlass-vorlesungen-und-abhandlungen-4993/3|title=Handschriftlicher, Nachlass, Vorlesungen und Abhandlungen.|website=Gutenberg Spiegel|access-date=1 December 2017|archive-date=20 August 2018|archive-url=https://web.archive.org/web/20180820043701/http://gutenberg.spiegel.de/buch/arthur-schopenhauers-handschriftlicher-nachlass-vorlesungen-und-abhandlungen-4993/3|url-status=live}}</ref>

Schopenhauer expressed regret that Spinoza stuck, for the presentation of his philosophy, with the concepts of [[scholasticism]] and [[Cartesian philosophy]], and tried to use geometrical proofs that do not hold because of vague and overly broad definitions. Bruno on the other hand, who knew much about nature and ancient literature, presented his ideas with Italian vividness, and is amongst philosophers the only one who comes near Plato's poetic and dramatic power of exposition.<ref name="Spinoza and Bruno" /><ref name="Presentation" />

Schopenhauer noted that their philosophies do not provide any ethics, and it is therefore very remarkable that Spinoza called his main work ''[[Ethics (Spinoza book)|Ethics]]''. In fact, it could be considered complete from the standpoint of life-affirmation, if one completely ignores morality and self-denial.<ref>{{Cite book|title=Abschnitt: Handschriftlicher Nachlaß|at=§ 588|quote=Es kann daher eine vollkommen wahre Philosophie geben, die ganz von der Verneinung des Lebens abstrahirt, diese ganz ignorirt.}}</ref> It is yet even more remarkable that Schopenhauer mentions Spinoza as an example of the denial of the will, if one uses the French biography by Jean Maximilien Lucas<ref>{{cite web|url=https://fr.wikisource.org/wiki/Vie_de_Spinoza|title=Vie de Spinoza – Wikisource|website=fr.wikisource.org}}</ref> as the key to ''[[Tractatus de Intellectus Emendatione]]''.<ref>{{Cite book|title=The World as Will and Representation|at=§ 68|quote=We might to a certain extent regard the well-known French biography of Spinoza as a case in point, if we used as a key to it that noble introduction to his very insufficient essay, "De Emendatione Intellects", a passage which I can also recommend as the most effectual means I know of stilling the storm of the passions.}}</ref>

=== Immanuel Kant ===
{{See also|Critique of the Kantian philosophy|Schopenhauer's criticism of Kant's schemata}}[[File:Immanuel Kant portrait c1790.jpg|thumb|right|Schopenhauer's philosophy took Kant's work as its foundation. While he praised Kant's greatness, he nonetheless included a highly detailed criticism of Kantian philosophy as an appendix to ''The World as Will and Representation''.]]
Kant's influence on Schopenhauer's development, personally as well as in philosophy, was extensive. Kant's philosophy lies at the foundation of Schopenhauer's, and he had high praise for the [[Critique of Pure Reason#Transcendental Aesthetic|Transcendental Aesthetic]] section of Kant's ''Critique of Pure Reason''. Schopenhauer maintained that Kant stands in the same relation to philosophers such as Berkeley and Plato, as Copernicus to [[Hicetas]], [[Philolaus]], and [[Aristarchus of Samos]]: Kant succeeded in demonstrating what previous philosophers merely asserted.

Schopenhauer writes about Kant's influence on his work in the preface to the second edition of ''The World as Will and Representation'':

{{Blockquote|I have already explained in the preface to the first edition, that my philosophy is founded on that of Kant, and therefore presupposes a thorough knowledge of it. I repeat this here. For Kant's teaching produces in the mind of everyone who has comprehended it a fundamental change which is so great that it may be regarded as an intellectual new-birth. It alone is able really to remove the inborn realism which proceeds from the original character of the intellect, which neither [[George Berkeley|Berkeley]] nor [[Nicolas Malebranche|Malebranche]] succeed in doing, for they remain too much in the universal, while Kant goes into the particular, and indeed in a way that is quite unexampled both before and after him, and which has quite a peculiar, and, we might say, immediate effect upon the mind in consequence of which it undergoes a complete undeception, and forthwith looks at all things in another light. Only in this way can any one become susceptible to the more positive expositions which I have to give. On the other hand, he who has not mastered the Kantian philosophy, whatever else he may have studied, is, as it were, in a state of innocence; that is to say, he remains in the grasp of that natural and childish realism in which we are all born, and which fits us for everything possible, with the single exception of philosophy.<ref>{{Cite book|title=World as Will and Representation|last=Arthur Schopenhauer|volume=1, Preface of the Second Edition}}</ref>}}

In his study room, one bust was of [[Gautama Buddha|Buddha]], the other was of Kant.<ref>{{Cite book|title=Schopenhauer. Pessimist and Pagan.|last=Jerauld McGill|first=Vivian|year=1931|page=320}}</ref> The bond that Schopenhauer felt with the philosopher is demonstrated in an unfinished poem he dedicated to Kant (included in volume 2 of the ''Parerga''):

{{blockquote|With my eyes I followed thee into the blue sky,<br />And there thy flight dissolved from view.<br />Alone I stayed in the crowd below,<br />Thy word and thy book my only solace.—<br />Through the strains of thy inspiring words<br />I sought to dispel the dreary solitude.<br />Strangers on all sides surround me.<br />The world is desolate and life interminable.<ref>''Parerga and Paralipomena: Short Philosophical Essays, Volume 2'', trans. Payne, p. 655–656.</ref>}}

Schopenhauer dedicated one fifth of his main work, ''The World as Will and Representation'', to a detailed [[Critique of the Kantian philosophy|criticism of the Kantian philosophy]].

Schopenhauer praised Kant for his distinction between appearance and the [[thing-in-itself]], whereas the general consensus in [[German idealism]] was that this was the weakest spot of Kant's theory,<ref name=":0">{{Cite book|title=Introduction to "On the Fourfold Root of the Principle of Sufficient Reason"|author1=David E. Cartwright |author2=Edward E. Erdmann|publisher=Cambridge University Press|pages=xvi–xvii|quote=He had also rehearsed for the first time his physiological arguments for the intellectual nature of intuition [Anschauung, objective perception] in his "On Vision and Colours", and he had discussed how his philosophy was corroborated by the sciences in "On Will in Nature". ... Like the German Idealists, Schopenhauer was convinced that Kant's great unknown, the thing in itself, is the weak point of the critical philosophy.}}</ref> since, according to Kant, causality can find application on objects of experience only, and consequently, things-in-themselves cannot be the cause of appearances. The inadmissibility of this reasoning was also acknowledged by Schopenhauer. He insisted that this was a true conclusion, drawn from false premises.<ref>{{Cite book|title=The World as Will and Representation|last=Schopenhauer|first=Arthur|volume=1 Criticism of the Kantian philosophy|translator-last=J. Kemp|quote=With the proof of the thing in itself it has happened to Kant precisely as with that of the a priori nature of the law of causality. Both doctrines are true, but their proof is false. They thus belong to the class of true conclusions from false premises.}}</ref>

=== Post-Kantian school ===
The leading figures of [[German idealism|post-Kantian philosophy]]—[[Johann Gottlieb Fichte]], Schelling and Hegel—were not respected by Schopenhauer. He argued that they were not philosophers at all, for they lacked "the first requirement of a philosopher, namely a seriousness and honesty of inquiry."<ref>''Parerga and Paralipomena,'' Vol. 1, Appendix to "Sketch of a History of the Doctrine of the Ideal and the Real," trans. E. J. Payne (Oxford, 1974), p. 21.</ref> Rather, they were merely sophists who, excelling in the art of beguiling the public, pursued their own selfish interests (such as professional advancement within the university system). Diatribes against the alleged vacuity, dishonesty, pomposity, and self-interest of these contemporaries are to be found throughout Schopenhauer's published writings. The following passage is an example:

{{Blockquote|All this explains the painful impression with which we are seized when, after studying genuine thinkers, we come to the writings of Fichte and Schelling, or even to the presumptuously scribbled nonsense of Hegel, produced as it was with a boundless, though justified, confidence in German stupidity. With those genuine thinkers one always found an ''honest'' investigation of truth and just as ''honest'' an attempt to communicate their ideas to others. Therefore whoever reads Kant, Locke, Hume, Malebranche, Spinoza, and Descartes feels elevated and agreeably impressed. This is produced through communion with a noble mind which has and awakens ideas and which thinks and sets one thinking. The reverse of all this takes place when we read the above-mentioned three German sophists. An unbiased reader, opening one of their books and then asking himself whether this is the tone of a thinker wanting to instruct or that of a charlatan wanting to impress, cannot be five minutes in any doubt; here everything breathes so much of ''dishonesty''.<ref>''Parerga and Paralipomena,'' Vol. 1, Appendix to "Sketch of a History of the Doctrine of the Ideal and the Real," trans. E. J. Payne (Oxford, 1974), p. 23.</ref>}}

Schopenhauer deemed Schelling the most talented of the three and wrote that he would recommend his "elucidatory paraphrase of the highly important doctrine of Kant" concerning the intelligible character, if he had been honest enough to admit he was parroting Kant, instead of hiding this relation in a cunning manner.<ref>{{Cite book|title=On the Freedom of the Will|last=Schopenhauer|first=Arthur|page=82}}</ref>

Schopenhauer reserved his most unqualified damning condemnation for Hegel, whom he considered less worthy than Fichte or Schelling. Whereas Fichte was merely a windbag (''Windbeutel''), Hegel was a "commonplace, inane, loathsome, repulsive, and ignorant charlatan."<ref>''Parerga and Paralipomena'', Vol. I, "Fragments for the History of Philosophy", Sec. 13, trans. E. J. Payne (Oxford, 1974), p. 96.</ref> The philosophers [[Karl Popper]] and [[Mario Bunge]] agreed with this distinction.<ref>{{Cite journal|title=The Open Society and Her Enemies|journal=Nature|volume=157|issue=3987|last=Popper|first=Karl|year=1946|page=52|bibcode=1946Natur.157..387R|doi=10.1038/157387a0|s2cid=4074331}}</ref><ref name="Bunge1">{{cite web | author = Bunge, Mario | year = 2020 | title = Mario Bunge nos dijo: "Se puede ignorar la filosofía, pero no evitarla" | url = https://www.filco.es/mario-bunge-no-evitar-filosofia/ | publisher = Filosofía&Co | access-date = 26 May 2020 | archive-date = 15 November 2022 | archive-url = https://web.archive.org/web/20221115013405/https://filco.es/mario-bunge-no-evitar-filosofia/ | url-status = live }}</ref> Hegel, Schopenhauer wrote in the preface to his ''Two Fundamental Problems of Ethics'', not only "performed no service to philosophy, but he has had a detrimental influence on philosophy, and thereby on German literature in general, really a downright stupefying, or we could even say a pestilential influence, which it is therefore the duty of everyone capable of thinking for himself and judging for himself to counteract in the most express terms at every opportunity."<ref>''The Two Fundamental Problems of Ethics'', Preface to the First Edition, trans. Christopher Janaway (Cambridge, 2009), p. 15.</ref>

== Influence and legacy ==
[[File:Arthur_Schopenhauer_by_Elisabet_Ney.jpg|thumb|[[Arthur Schopenhauer (sculpture)|Sculpture of Schopenhauer]] by [[Elisabeth Ney]]]]
Schopenhauer remained the most influential German philosopher until the [[First World War]].<ref name=Weltschmerz>{{Cite book|title=Weltschmerz, Pessimism in German Philosophy, 1860–1900|last=Beiser|first=Frederick C.|publisher=Oxford University Press|year=2008|isbn=978-0-19-876871-5|location=Oxford|pages=14–16|quote=Arthur Schopenhauer was the most famous and influential philosopher in Germany from 1860 until the First World War. ... Schopenhauer had a profound influence on two intellectual movements of the late 19th century that were utterly opposed to him: neo-Kantianism and positivism. He forced these movements to address issues they would otherwise have completely ignored, and in doing so he changed them markedly. ... Schopenhauer set the agenda for his age.}}</ref> His philosophy was a starting point for a new generation of philosophers including [[Julius Bahnsen]], [[Paul Deussen]], Lazar von Hellenbach, [[Karl Robert Eduard von Hartmann]], Ernst Otto Lindner, [[Philipp Mainländer]], [[Friedrich Nietzsche]], [[Olga Plümacher]] and [[Agnes Taubert]]. His legacy shaped the intellectual debate, and forced movements that were utterly opposed to him, [[neo-Kantianism]] and [[positivism]], to address issues they would otherwise have completely ignored, and in doing so he changed them markedly.<ref name="Weltschmerz" /> The French writer [[Guy de Maupassant]] commented that "to-day even those who execrate him seem to carry in their own souls particles of his thought".<ref>Beside Schopenhauer's Corpse</ref> Other philosophers of the 19th century who cited his influence include [[Hans Vaihinger]], [[Johannes Volkelt]], [[Vladimir Solovyov (philosopher)|Vladimir Solovyov]] and [[Otto Weininger]].

Schopenhauer was well read by physicists, most notably [[Albert Einstein]], [[Erwin Schrödinger]], [[Wolfgang Pauli]]<ref>{{Cite book|title=A Peek behind the Veil of Maya: Einstein, Schopenhauer, and the Historical Background of the Conception of Space as a Ground for the Individuation of Physical Systems|first=Don|last=Howard|publisher=University of Pittsburgh Press|year=1997|quote=Pauli greatly admired Schopenhauer. ... Pauli wrote sympathetically about extrasensory perception, noting approvingly that "even such a thoroughly critical philosopher as Schopenhauer not only regarded parapsychological effects going far beyond what is secured by scientific evidence as possible, but even considered them as a support for his philosophy".}}</ref> and [[Ettore Majorana]].<ref name=Majorana>{{Cite book|title=Ettore Majorana: Scientific Papers|last=Bassani|first=Giuseppe-Franco|date=15 December 2006|publisher=Springer|isbn=978-3-540-48091-4|editor-last=Società Italiana di Fisica|page=xl|quote=His interest in philosophy, which had always been great, increased and prompted him to reflect deeply on the works of various philosophers, in particular Schopenhauer.}}</ref> Einstein described Schopenhauer's thoughts as a "continual consolation" and called him a genius.<ref>{{Cite book|title=Einstein: His Life and Universe|last=Isaacson|first=Walter|publisher=Simon & Schuster|year=2007|isbn=978-0-7432-6474-7|location=New York|page=367}}</ref> In his Berlin study three figures hung on the wall: [[Michael Faraday]], [[James Clerk Maxwell]] and Schopenhauer.<ref>Howard (1997). p. 87</ref> [[Konrad Wachsmann]] recalled: "He often sat with one of the well-worn Schopenhauer volumes, and as he sat there, he seemed so pleased, as if he were engaged with a serene and cheerful work."<ref>Howard (1997). p. 92</ref>

When Schrödinger discovered Schopenhauer ("the greatest savant of the West") he considered switching his study of physics to philosophy.<ref>{{Cite book|title=Einstein's Dice and Schrödinger's Cat: How Two Great Minds Battled Quantum Randomness to Create a Unified Theory of Physics|last=Halpern|first=Paul|year=2015|isbn=978-0-465-04065-0|page=189|publisher=Basic Books }}</ref> He maintained the idealistic views during the rest of his life.<ref>Howard (1997). p. 132</ref> Pauli accepted the main tenet of Schopenhauer's metaphysics, that the [[thing-in-itself]] is will.<ref>{{Cite web|url=https://www.academia.edu/6149849|title=Schopenhauers Metaphysics and Contemporary Quantum Theory|last=Raymond B. Marcin|quote=David Lindorff referred to Schopenhauer as Pauli's "favorite philosopher", and Pauli himself often expressed his agreement with the main tenet of Schopenhauer's philosophy. ... Suzanne Gieser cited a 1952 letter from Pauli to Carl Jung, in which Pauli indicated that, while he accepted Schopenhauer's main tenet that the thing-in-itself of all reality is will.}}{{Dead link|date=August 2023 |bot=InternetArchiveBot |fix-attempted=yes }}</ref>

But most of all Schopenhauer is famous for his influence on artists. [[Richard Wagner]] became one of the earliest and most famous adherents of the Schopenhauerian philosophy.<ref>See e.g. Magee (2000) 276–278.</ref> The admiration was not mutual, and Schopenhauer proclaimed: "I remain faithful to Rossini and Mozart!"<ref>{{Cite book|title=The Invention of Beethoven and Rossini: Historiography, Analysis, Criticism|last=Nicholas Mathew, Benjamin Walton|page=296}}</ref> So he [[List of nicknames of philosophers|has been nicknamed]] "the artist's philosopher".<ref name=iep/> See also [[Tristan und Isolde#Influence of Schopenhauer on Tristan und Isolde|Influence of Schopenhauer on ''Tristan und Isolde'']].

{{Css Image Crop|Image = DAN-28a-Danzig-500MIL Mark (1923).jpg|bSize = 235|cWidth = 235|cHeight = 133|oTop = 2|oLeft = 0|Location = right|Description= Schopenhauer depicted on a 500 million Danzig [[Papiermark#Danzig|papiermark]] note (1923)}}

Under the influence of Schopenhauer, [[Leo Tolstoy]] became convinced that the truth of all religions lies in self-renunciation. When he read Schopenhauer's philosophy, Tolstoy exclaimed "at present I am convinced that Schopenhauer is the greatest genius among men. ... It is the whole world in an incomparably beautiful and clear reflection."<ref>Tolstoy's letter to Afanasy Fet on 30 August 1869. "Do you know what this summer has meant for me? Constant raptures over Schopenhauer and a whole series of spiritual delights as I've never experienced before. I have brought all of his works and read him over and over, Kant too by the way. Assuredly no student has ever learned and discovered so much in one semester as I have during this summer. I do not know if I shall ever change my opinion, but at present I am convinced that Schopenhauer is the greatest genius among men. You say he is so-so, he has written a few things on philosophy? What is so-so? It is the whole world in an incomparably beautiful and clear reflection. I have started to translate him. Won't you help me? Indeed, I cannot understand how his name can be unknown. The only explanation for this can only be the one he so often repeats, that is, that there is scarcely anyone but idiots in the world."</ref> He said that what he has written in ''[[War and Peace]]'' is also said by Schopenhauer in ''The World as Will and Representation''.<ref>{{cite journal|url=https://muse.jhu.edu/article/316432|title=Quietism from the Side of Happiness: Tolstoy, Schopenhauer, War and Peace|last=Thompson|first=Caleb|journal=Common Knowledge|year=2009|volume=15|issue=3|pages=395–411|doi=10.1215/0961754X-2009-020|s2cid=145535267|archive-date=4 December 2021|access-date=15 November 2017|archive-url=https://web.archive.org/web/20211204061944/https://muse.jhu.edu/article/316432|url-status=live|url-access=subscription}}</ref>

[[Jorge Luis Borges]] remarked that the reason he had never attempted to write a systematic account of his world view, despite his penchant for philosophy and metaphysics in particular, was because Schopenhauer had already written it for him.<ref>{{cite book |last=Magee |first=Bryan |title=Confessions of a Philosopher |year=1997 |page=413}}</ref>

Other figures in literature who were strongly influenced by Schopenhauer were [[Thomas Mann]], [[Thomas Hardy]], [[Afanasy Fet]], [[Joris-Karl Huysmans|J.-K. Huysmans]] and [[George Santayana]].<ref>{{Cite journal|title=Santayana and Schopenhauer|last=Caleb Flamm|first=Matthew|journal=Transactions of the Charles S. Peirce Society|volume=38|issue=3|pages=413–431|quote=A thinker of whom it is well known that Santayana had an early, deep admiration, namely, Schopenhauer|jstor = 40320900|year=2002}}</ref> In [[Herman Melville|Herman Melville's]] final years, while he wrote ''[[Billy Budd]]'', he read Schopenhauer's essays and marked them heavily. Scholar Brian Yothers notes that Melville "marked numerous misanthropic and even suicidal remarks, suggesting an attraction to the most extreme sorts of solitude, but he also made note of Schopenhauer's reflection on the moral ambiguities of genius."<ref>{{cite book |last1=Yothers |first1=Brian |title=Sacred Uncertainty: Religious Difference and The Shape of Melville's Career |date=2015 |publisher=Northwestern University Press |location=Evanston, Illinois |isbn=978-0-8101-3071-5 |page=13}}</ref> Schopenhauer's attraction to and discussions of both Eastern and Western religions in conjunction with each other made an impression on Melville in his final years.

[[Sergei Prokofiev]], although initially reluctant to engage with works noted for their pessimism, became fascinated with Schopenhauer after reading ''Aphorisms on the Wisdom of Life'' in ''Parerga and Paralipomena''. "With his truths Schopenhauer gave me a spiritual world and an awareness of happiness."<ref>{{Cite book|title=Sergey Prokofiev and His World|last=Morrison|first=Simon|publisher=Princeton University Press|year=2008|isbn=978-0-691-13895-4|pages=19, 20}}</ref>

Nietzsche owed the awakening of his philosophical interest to reading ''The World as Will and Representation'' and admitted that he was one of the few philosophers that he respected, dedicating to him his essay "Schopenhauer als Erzieher",<ref>[[s:Schopenhauer as Educator|Schopenhauer as Educator]]</ref> one of his ''[[Untimely Meditations]]''.

[[File:DBP 1988 1357 Arthur Schopenhauer.jpg|thumb|Commemorative stamp of the Deutsche Bundespost, 1988]]

Early in his career, [[Ludwig Wittgenstein]] adopted Schopenhauer's epistemological idealism, and some traits of Schopenhauer's influence (particularly Schopenhauerian transcendentalism) can be observed in the ''[[Tractatus Logico-Philosophicus]]''.<ref>{{cite book | author = Glock, Hans-Johann | year = 2017 | title = A Companion to Wittgenstein | page = [https://books.google.com/books?id=WbfBDQAAQBAJ&pg=PA60 60] | location = Sussex, UK | publisher = Wiley Blackwell}}</ref><ref>{{cite book | author = Glock, Hans-Johann | year = 2000 | title = The Cambridge Companion to Schopenhauer | page = [https://books.google.com/books?id=PnUF-UjhX_oC&pg=PA424 424] | location = New York, NY | publisher = Cambridge University Press}}</ref> Later on, Wittgenstein rejected epistemological [[transcendental idealism]] for [[Gottlob Frege]]'s conceptual [[Metaphysical realism|realism]]. In later years, Wittgenstein became highly dismissive of Schopenhauer, describing him as an ultimately shallow thinker.<ref name="Culture & Value, p. 24, 1933–4">Culture & Value, p.&nbsp;24, 1933–34</ref><ref>Malcolm, Norman. Ludwig Wittgenstein: A Memoir. Oxford University Press, 1958, p. 6</ref> His friend [[Bertrand Russell]] had a low opinion on the philosopher, and even came to attack him in his [[History of Western Philosophy (Russell)|''History of Western Philosophy'']] for hypocritically praising asceticism yet not acting upon it.<ref>{{cite book|last=Russell|first=Bertrand|title=History of Western Philosophy|year=1946|publisher=George Allen and Unwin |page=786}}</ref>

Opposite to Russell on the foundations of mathematics, the Dutch mathematician [[L. E. J. Brouwer]] incorporated Kant's and Schopenhauer's ideas in the philosophical school of [[intuitionism]], where mathematics is considered as a purely mental activity instead of an analytic activity wherein objective properties of reality are revealed. Brouwer was also influenced by Schopenhauer's metaphysics, and wrote an essay on mysticism.

Schopenhauer's philosophy features in the novel ''[[The Schopenhauer Cure]]'', by American existential psychiatrist and emeritus professor of psychiatry [[Irvin Yalom]].

Schopenhauer's philosophy, and the discussions on [[philosophical pessimism]] it has engendered, has been the focus of contemporary thinkers such as [[David Benatar]], [[Thomas Ligotti]], and [[Eugene Thacker]]. Their work also served as an inspiration for the popular HBO TV series ''[[True Detective]]'' as well as ''[[Life Is Beautiful]]''.<ref>{{Cite news|title=Writer Nic Pizzolatto on Thomas Ligotti and the Weird Secrets of 'True Detective'|url=https://www.wsj.com/articles/BL-SEB-79577|date=2014-02-12|work=The Wall Street Journal|archive-date=11 October 2023|access-date=14 September 2021|archive-url=https://web.archive.org/web/20231011030452/https://www.wsj.com/articles/BL-SEB-79577|url-status=live}}</ref> In this regard, Schopenhauer is sometimes considered the founding father of today's [[antinatalism]].<ref>M.Morioka [https://philpapers.org/rec/MORWIA-14'' What Is Antinatalism? and Other Essays''] {{Webarchive|url=https://web.archive.org/web/20230326032013/https://philpapers.org/rec/MORWIA-14 |date=26 March 2023 }}, pp.8–12.</ref>

Advocates of [[metaphysical idealism|idealism]] in contemporary [[analytic philosophy]] and [[neuroscience]] such as [[Bernardo Kastrup]] and [[Christof Koch]] owe their philosophical system, in part, to the metaphysics of Schopenhauer.<ref>Kastrup, Bernardo ''Decoding Schopenhauer’s Metaphysics: The Key to Understanding How It Solves the Hard Problem of Consciousness and the Paradoxes of Quantum Mechanics'' Iff Books (July 31, 2020)</ref><ref>Koch, Christof ''Then I Am Myself the World: What Consciousness Is and How to Expand It'' Basic Books (May 7, 2024) pp. 49, 127, 132, 139, 209, 244.</ref>

== Selected bibliography ==
<!--older texts use "Ueber" instead of "Über"-->
* ''[[On the Fourfold Root of the Principle of Sufficient Reason]]'' (''Ueber die vierfache Wurzel des Satzes vom zureichenden Grunde''), 1813 (revised and enlarged: 1847)
* ''[[On Vision and Colours]]'' (''Ueber das Sehn und die Farben''), 1816 {{ISBN|978-0-85496-988-3}} (revised and enlarged: 1854)
* ''Theory of Colours'' (''Theoria colorum physiologica''), 1830
* ''[[The World as Will and Representation]]'' (alternatively translated as ''The World as Will and Idea''; original German is ''Die Welt als Wille und Vorstellung''): vol. 1, 1818–1819, vol. 2, 1844 [[Världen som vilja och föreställning]] (2nd edition: 1844, 3rd edition: 1859)
** Vol. 1 Dover edition 1966, {{ISBN|978-0-486-21761-1}}
** Vol. 2 Dover edition 1966, {{ISBN|978-0-486-21762-8}}
** Peter Smith Publisher hardcover set 1969, {{ISBN|978-0-8446-2885-1}}
** Everyman Paperback combined abridged edition (290 pp.) {{ISBN|978-0-460-87505-9}}
** The Longman Library of Primary Sources in Philosophy, vol. 1, 2008; vol. 2, 2010. Title translated as ''The World as Will and Presentation'', rather than ''Representation''.
* ''[[The Art of Being Right]]'' (''Eristische Dialektik: Die Kunst, Recht zu Behalten''), 1831
* ''On the Will in Nature'' (''Ueber den Willen in der Natur''), 1836 {{ISBN|978-0-85496-999-9}} (revised and enlarged: 1854)
* ''[[On the Freedom of the Will]]'' (''Ueber die Freiheit des menschlichen Willens''), 1838 {{ISBN|978-0-631-14552-3}}
* ''[[On the Basis of Morality]]'' (''Ueber die Grundlage der Moral''), 1839
* {{cite book | last = Schopenhauer | first = Arthur | author-link = Arthur Schopenhauer |translator-last1=Cartwright |translator-first1=David E. |translator-last2=Erdmann |translator-first2=Edward E. | title = The Two Fundamental Problems of Ethics | publisher=[[Oxford University Press]] | date = 2010| location = London|isbn=9780199297221}} 1841 (revised and enlarged: 1860) Contains ''[[On the Freedom of the Will]]'' and ''[[On the Basis of Morality]].''
* {{cite book |last=Schopenhauer |first=Arthur |author-link=Arthur Schopenhauer |title=Die beiden Grundprobleme der Ethik, behandelt in zwei akademischen Preisschriften |publisher=Johann Christian Hermannsche Buchandlung |date=September 1840 |orig-date=Stated date: 1841. |location=Frankfurt am Main |language=German |access-date=2024-04-15 |url=https://archive.org/details/diebeidengrundpr00scho/}} Freely [[iarchive:diebeidengrundpr00scho/|available]] from [[Internet Archive]]. Contains ''Preisschrift über die Freiheit des Willens'' and ''Preisschrift über die Grundlage der Moral''.
* ''[[Parerga and Paralipomena]]'' (2 vols., 1851) – Reprint: (Oxford: Clarendon Press) (2 vols., 1974) (English translation by E. F. J. Payne)
** Printings:
*** 1974 Hardcover, by ISBN
**** Vols. 1 and 2, {{ISBN|978-0-19-519813-3}},
**** Vol. 1, ISBN
**** Vol. 2, {{ISBN|978-0-19-824527-8}},
*** 1974–1980 Paperback, Vol. 1, {{ISBN|978-0-19-824634-3}}, Vol. 2, {{ISBN|978-0-19-824635-0}},
*** 2001 Paperback, Vol. 1, {{ISBN|978-0-19-924220-7}}, Vol. 2, {{ISBN|978-0-19-924221-4}}
** ''Essays and Aphorisms'', being excerpts from Volume 2 of ''Parerga und Paralipomena'', selected and translated by R. J. Hollingdale, with Introduction by R J Hollingdale, Penguin Classics, 1970, Paperback 1973: {{ISBN|978-0-14-044227-4}}
* ''An Enquiry concerning Ghost-seeing, and what is connected therewith (Versuch über das Geistersehn und was damit zusammenhangt)'', 1851
* ''Manuscript Remains'', (6 Volumes), Volume 2 published by Berg Publishers Ltd., {{ISBN|978-0-85496-539-7}}

=== Online ===
* {{gutenberg author|id=Arthur+Schopenhauer | name=Arthur Schopenhauer}}
* ''[http://coolhaus.de/art-of-controversy/ The Art of Controversy (Die Kunst, Recht zu behalten)] {{Webarchive|url=https://web.archive.org/web/20021215085620/http://www.coolhaus.de/art-of-controversy/ |date=15 December 2002 }}''. (bilingual) [''[[The Art of Being Right]]'']
* ''[http://librivox.org/studies-in-pessimism-by-arthur-schopenhauer/ Studies in Pessimism]'' – audiobook from [[LibriVox]]
* ''The World as Will and Idea'' at the [[Internet Archive]]:
** ''[https://archive.org/details/theworldaswillan01schouoft Volume I]''
** ''[https://archive.org/details/theworldaswill02schouoft Volume II]''
** ''[https://archive.org/details/theworldaswillan03schouoft Volume III]''
* "On the fourfold root of the principle of sufficient reason" and "On the will in nature". Two essays:
** [https://archive.org/details/onthefourfoldroo00schouoft Internet Archive]. Translated by Mrs. Karl Hillebrand (1903).
** [https://dlxs2.library.cornell.edu/cgi/t/text/text-idx?c=cdl;idno=cdl322 Cornell University Library Historical Monographs Collection]. Reprinted by Cornell University Library Digital Collections
* [https://web.archive.org/web/20081029052257/http://www.schopenhauersource.org/type_list.php?type=manuscript Facsimile edition of Schopenhauer's manuscripts] in [https://web.archive.org/web/20150424113720/http://www.schopenhauersource.org/ SchopenhauerSource]
* ''[https://web.archive.org/web/20080304165547/http://ebooks.adelaide.edu.au/s/schopenhauer/arthur/essays/complete.html Essays of Schopenhauer] ''

== See also ==
{{Portal|Philosophy}}
{{cols|colwidth=26em}}
* [[Antinatalism]]
* [[Existential nihilism]]
* [[Eye of a needle]]
* [[God in Buddhism]]
* [[Massacre of the Innocents (Reni)|''Massacre of the Innocents'' (Guido Reni)]]
* [[Mortal coil]]
* [[Nihilism]]
* [[Post-Schopenhauerian pessimism]]
{{colend}}

==Footnotes==
{{notelist}}

== References ==
<references>

<ref name="Benatar-2006">{{cite book |last=Benatar |first=David |date=2006 |author-link=David Benatar |title=Better Never to Have Been: The Harm of Coming into Existence |title-link=Better Never to Have Been |publisher=Oxford University Press |isbn=978-0199296422}}</ref>

<ref name=Cartwright-2010>{{cite book|last=Cartwright|first=David E.|year=2010|title=Schopenhauer: A Biography |publisher=Cambridge University Press |isbn=978-0-521-82598-6}}</ref>

<ref name="Janaway-2025">{{cite journal |last=Janaway |first=Christopher |date=2025 |title=Schopenhauer and anti-natalism |journal=British Journal for the History of Philosophy |pages=1–25 |doi=10.1080/09608788.2025.2589084 |doi-access=free}}</ref>

<ref name="Schopenhauer-2015">{{cite book |author-last=Schopenhauer |author-first=Arthur |date=2015 |title=Parerga and Paralipomena: Volume 2: Short Philosophical Essays |author-link=Arthur Schopenhauer |editor-last1=Janaway |editor-first1=Christopher |translator-last1=Caro |translator-first1= Adrian Del |orig-date=1851 |title-link=Parerga and Paralipomena |place=Cambridge |publisher=Cambridge University Press |isbn=978-1108436526 }}</ref>

<ref name="Schopenhauer-2018">{{cite book |author-last=Schopenhauer |author-first=Arthur |date=2018 |title=The World as Will and Representation |orig-date=1844 |volume=2 |place=Cambridge |publisher=Cambridge University Press |doi=10.1017/9780511843112 |isbn=978-0-521-87034-4 |editor-last=Welchman |editor-first=Alistair |editor2-last=Janaway |editor2-first=Christopher |editor3-last=Norman |editor3-first=Judith |author-link1=Arthur Schopenhauer |title-link=The World as Will and Representation}}</ref>

</references>

=== Sources ===
{{Refbegin}}
* Albright, Daniel (2004) ''Modernism and Music: An Anthology of Sources''. University of Chicago Press. {{ISBN|978-0-226-01267-4}}
* [[Frederick C. Beiser|Beiser, Frederick C.]], ''Weltschmerz: Pessimism in German Philosophy, 1860–1900'' (Oxford: Oxford University Press, 2016).
* {{cite book|last=Cartwright|first=David E.|year=2010|title=Schopenhauer: A Biography|url=https://books.google.com/books?id=meD1bGAjO6wC&pg=PA30|publisher=Cambridge University Press|isbn=978-0-521-82598-6}}
* {{cite book |last=Clarke |first=John James |year=1997 |title=Oriental Enlightenment: The Encounter Between Asian and Western Thought |url=https://books.google.com/books?id=8YOGAgAAQBAJ&pg=PA67 |location=[[Abingdon-on-Thames|Abingdon, Oxfordshire]] |publisher=[[Routledge]] |isbn=978-0-415-13376-0 }}
* Hannan, Barbara, ''The Riddle of the World: A Reconsideration of Schopenhauer's Philosophy'' (Oxford: Oxford University Press, 2009).
* [[Bryan Magee|Magee, Bryan]], ''Confessions of a Philosopher'', Random House, 1997, {{ISBN|978-0-375-50028-2}}. Chapters 20, 21.
* [[Rüdiger Safranski|Safranski, Rüdiger]] (1990) ''[[Schopenhauer and the Wild Years of Philosophy]]''. Harvard University Press, {{ISBN|978-0-674-79275-3}}; orig. German ''Schopenhauer und Die wilden Jahre der Philosophie'', Carl Hanser Verlag (1987)
* [[Thomas Mann]] editor, ''The Living Thoughts of Schopenhauer'', Longmans Green & Co., 1939
{{Refend}}

== Further reading ==
=== Biographies ===
* [[Frederick Copleston|Copleston, Frederick]], ''Arthur Schopenhauer, Philosopher of Pessimism'' (Burns, Oates & Washbourne, 1946)
* Damm, O. F., ''Arthur Schopenhauer – eine Biographie'' (Reclam, 1912)
* Fischer, Kuno, ''Arthur Schopenhauer'' (Heidelberg: Winter, 1893); revised as ''Schopenhauers Leben, Werke und Lehre'' (Heidelberg: Winter, 1898).
* Grisebach, Eduard, ''Schopenhauer – Geschichte seines Lebens'' (Berlin: Hofmann, 1876).
* Hamlyn, D. W., ''Schopenhauer'', London: Routledge & Kegan Paul (1980, 1985)
* Hasse, Heinrich, ''Schopenhauer''. (Reinhardt, 1926)
* Hübscher, Arthur, ''Arthur Schopenhauer – Ein Lebensbild'' (Leipzig: Brockhaus, 1938).
* [[Thomas Mann|Mann, Thomas]], ''Schopenhauer'' (Bermann-Fischer, 1938)
* [[Jack Matthews (author)|Matthews, Jack]], ''Schopenhauer's Will: Das Testament'', Nine Point Publishing, 2015. {{ISBN|978-0-9858278-8-5}}. A recent creative biography by philosophical novelist [[Jack Matthews (author)|Jack Matthews]].
* Safranski, Rüdiger, ''Schopenhauer und die wilden Jahre der Philosophie – Eine Biographie'', hard cover Carl Hanser Verlag, München 1987, {{ISBN|978-3-446-14490-3}}, pocket edition Fischer: {{ISBN|978-3-596-14299-6}}.
* Safranski, Rüdiger, ''[[Schopenhauer and the Wild Years of Philosophy]]'', trans. Ewald Osers (London: Weidenfeld and Nicolson, 1989)
* Schneider, Walther, ''Schopenhauer – Eine Biographie'' (Vienna: Bermann-Fischer, 1937).
* Wallace, William, ''Life of Arthur Schopenhauer'' (London: Scott, 1890; repr., St. Clair Shores, Mich.: Scholarly Press, 1970)
* {{Cite book|title=Arthur Schopenhauer: The Life and Thought of Philosophy's Greatest Pessimist|last=Bather Woods|first=David|publisher=University of Chicago Press|year=2025|isbn=9780226829760|location=Chicago, IL}}
* Zimmern, Helen, ''[https://archive.org/stream/arthurschopenha00zimmuoft#page/n7/mode/2up Arthur Schopenhauer: His Life and His Philosophy]'' (London: Longmans, Green & Co, 1876)

=== Other books ===
* [[Urs App|App, Urs]]. [https://sino-platonic.org/complete/spp200_schopenhauer.pdf Arthur Schopenhauer and China. ''Sino-Platonic Papers'' Nr. 200 (April 2010)] (PDF, 8.7&nbsp;Mb PDF, 164 p.). Contains extensive appendixes with transcriptions and English translations of Schopenhauer's early notes about Buddhism and Indian philosophy.
* App, Urs, ''Schopenhauers Kompass. Die Geburt einer Philosophie.'' UniversityMedia, Rorschach/ Kyoto 2011. {{ISBN|978-3-906000-02-2}}
* Atwell, John. ''Schopenhauer on the Character of the World, The Metaphysics of Will''.
* Atwell, John, ''Schopenhauer, The Human Character''.
* Edwards, Anthony. ''An Evolutionary Epistemological Critique of Schopenhauer's Metaphysics''. 123 Books, 2011.
* [[Patrick Gardiner|Gardiner, Patrick]], 1963. ''Schopenhauer''. Penguin Books.
* Janaway, Christopher, 2002. ''Schopenhauer: A Very Short introduction''. Oxford University Press. {{ISBN|978-0192802590}}
* Janaway, Christopher, 2003. ''Self and World in Schopenhauer's Philosophy''. Oxford University Press. {{ISBN|978-0-19-825003-6}}
* [[Bryan Magee|Magee, Bryan]], ''The Philosophy of Schopenhauer'', Oxford University Press (1988, revised and enlarged 1997). {{ISBN|978-0-19-823722-8}}
* Marcin, Raymond B. ''In Search of Schopenhauer's Cat: Arthur Schopenhauer's Quantum-Mystical Theory of Justice''. Washington, D.C.: The Catholic University of America Press, 2005. {{ISBN|978-0813214306}}
* Norberg, Jakob, [https://www.cambridge.org/core/books/schopenhauers-politics/CB3FAB9762A468BD306915D12E5B88F8 Schopenhauer's Politics]
* Mannion, Gerard, "Schopenhauer, Religion and Morality – The Humble Path to Ethics", Ashgate Press, New Critical Thinking in Philosophy Series, 2003, 314pp.
* [[Thomas Whittaker (metaphysician)|Whittaker, Thomas]], [https://gutenberg.org/ebooks/38283 Schopenhauer]
* [[Helen Zimmern|Zimmern, Helen]], ''[[s:Arthur Schopenhauer, his Life and Philosophy|Arthur Schopenhauer, his Life and Philosophy]]'', London, [[Longman|Longman, and Co.]], 1876.
* [[Bernardo Kastrup|Kastrup, Bernardo]]. ''Decoding Schopenhauer's Metaphysics – The key to understanding how it solves the hard problem of consciousness and the paradoxes of quantum mechanics.'' Winchester/Washington, iff Books, 2020.
* [[Alain de Botton|de Botton, Alain]]: ''[[The Consolations of Philosophy]]''. Hamish Hamilton, London 2000. {{ISBN|0-14-027661-0}} (Chapter: ''Consolation for a Broken Heart'').

=== Articles ===
* {{Cite journal | doi = 10.2307/1399616 | last1 = Abelson | first1 = Peter | year = 1993 | title = Schopenhauer and Buddhism | url = http://ccbs.ntu.edu.tw/FULLTEXT/JR-PHIL/peter2.htm | journal = Philosophy East and West | volume = 43 | issue = 2 | pages = 255–78 | jstor = 1399616 | access-date = 25 October 2007 | archive-url = https://web.archive.org/web/20110628204330/http://ccbs.ntu.edu.tw/FULLTEXT/JR-PHIL/peter2.htm | archive-date = 28 June 2011 | url-access = subscription }}
* Jiménez, Camilo, 2006, "[https://web.archive.org/web/20070702122520/http://www.avinus-magazin.eu/html/jimenez_-_der_junge_schopenhau.html Tagebuch eines Ehrgeizigen: Arthur Schopenhauers Studienjahre in Berlin]," ''Avinus Magazin'' (in German).
* Luchte, James, 2009, "[http://luchte.wordpress.com/the-body-of-sublime-knowledge-the-aesthetic-phenomenology-of-arthur-schopenhauer/ The Body of Sublime Knowledge: The Aesthetic Phenomenology of Arthur Schopenhauer]," ''Heythrop Journal'', Volume 50, Number 2, pp.&nbsp;228–242.
* Mazard, Eisel, 2005, "[http://www.pratyeka.org/schopenhauer/ Schopenhauer and the Empirical Critique of Idealism in the History of Ideas.] {{Webarchive|url=https://web.archive.org/web/20081029052835/http://www.pratyeka.org/schopenhauer/ |date=29 October 2008 }}" On Schopenhauer's (debated) place in the history of European philosophy and his relation to his predecessors.
* [[Sangharakshita]], 2004, "[https://web.archive.org/web/20040826122437/http://www.centrebouddhisteparis.org/En_Anglais/Sangharakshita_en_anglais/Aesthetic_appreciation/aesthetic_appreciation.html Schopenhauer and aesthetic appreciation.]"
* {{Cite journal | last1 = Young | first1 = Christopher | last2 = Brook | first2 = Andrew | year = 1994 | title = Schopenhauer and Freud | url = https://carleton.ca/~abrook/SCHOPENY.htm | journal = International Journal of Psychoanalysis | volume = 75 | pages = 101–18 | pmid = 8005756 }}
* [https://books.google.com/books?id=ungVAQAAIAAJ&pg=PP11 Oxenford's "Iconoclasm in German Philosophy," (See p. 388)]
* [[Eugene Thacker|Thacker, Eugene]], 2020. "A Philosophy in Ruins, An Unquiet Void." Introduction to Arthur Schopenhauer, ''On the Suffering of the World''. Repeater Books. {{ISBN|978-1-913462-03-1}}.

== External links ==
{{Sister project links|v=no|n=no|b=no|wikt=no|author=yes}}
* {{Gutenberg author |id=3648 | name=Arthur Schopenhauer}}
* {{Internet Archive author |sname=Arthur Schopenhauer}}
* {{Librivox author |id=165}}
* {{cite encyclopedia |author-last=Wicks |author-first=Robert |url=https://plato.stanford.edu/archives/spr2019/entries/schopenhauer/ |title=Arthur Schopenhauer |editor-last=Zalta |editor-first=Edward N. |editor-link=Edward N. Zalta |encyclopedia=[[Stanford Encyclopedia of Philosophy]] |date=Spring 2019 |publisher=[[Center for the Study of Language and Information]] |location=[[Stanford University]]}}
* {{cite IEP |url-id=schopenh |article=Arthur Schopenhauer |first=Mary |last=Troxell |date=11 May 2011}}
* {{cite IEP |last=Lemanski |first=Jens |date=13 January 2023 |url-id=schopenhauer-logic-and-dialectic |title=Arthur Schopenhauer: Logic and Dialectic}}
* [https://archive.org/details/cu31924029023327 Kant's philosophy as rectified by Schopenhauer]
* [https://web.archive.org/web/20110421040017/http://www.weple.org/timeline.html#ids=14631,12007,12598,700,10671,9518,37304,95184,&title=8%20German%20Philosophers Timeline of German Philosophers]
* [http://ljhammond.com/classics/cl1.htm#scho A Quick Introduction to Schopenhauer]
* Ross, Kelley L., 1998, "[http://www.friesian.com/arthur.htm Arthur Schopenhauer (1788–1860)]". Two short essays, on Schopenhauer's life and work, and on his dim view of academia.

{{Schopenhauer|state=uncollapsed}}
{{Metaphysics}}
{{Ethics}}
{{Aesthetics}}
{{Philosophical pessimism}}
{{Animal rights|advocates}}
{{Continental philosophy}}
{{Authority control}}

{{DEFAULTSORT:Schopenhauer, Arthur}}
[[Category:Arthur Schopenhauer| ]]
[[Category:1788 births]]
[[Category:1860 deaths]]
[[Category:19th-century atheists]]
[[Category:19th-century German philosophers]]
[[Category:Academic staff of the Humboldt University of Berlin]]
[[Category:German animal rights scholars]]
[[Category:Atheist philosophers]]
[[Category:Burials at Frankfurt Main Cemetery]]
[[Category:German epistemologists]]
[[Category:German atheists]]
[[Category:German ethicists]]
[[Category:German people of Dutch descent]]
[[Category:German scholars of Buddhism]]
[[Category:Idealists]]
[[Category:Kantian philosophers]]
[[Category:German philosophers of art]]
[[Category:Philosophers of pessimism]]
[[Category:Philosophers of psychology]]
[[Category:University of Göttingen alumni]]
[[Category:Writers from Gdańsk]]`,
`{{Short description|First caliph of Rashidun Caliphate from 632 to 634}}
{{About|the first caliph|3=Abu Bakr (disambiguation)}}
{{Use British English|date=September 2021}}
{{Use dmy dates|date=July 2022}}
{{Infobox royalty
| name = Abu Bakr{{break}}{{lang|ar|أَبُو بَكْر}}
| title = {{transliteration|engvar=gb|ar|[[List of caliphs|Khalifat Rasul Allah]]}}<br />{{transliteration|engvar=gb|ar|[[Islamic honorifics#Muhammad's companions|Raḍiya Ilāhu ʿAnhū]]}}
| image = 20131203 Istanbul 091.jpg
| image_size = 280px
| caption = Calligraphic seal featuring Abu Bakr's name, on display in the [[Hagia Sophia]], [[Istanbul]]
| succession = 1st [[Caliph]] of the [[Rashidun Caliphate]]
| reign = 8 June 632{{snd}}23 August 634
| cor-type = [[Coronation|Bayah]]
| predecessor = ''Position established''<br /> [[Muhammad]] (as [[Messenger of God]])
| successor = [[Umar]]
| spouse = {{plainlist|
* [[Qutaylah bint Abd al-Uzza|Qutayla bint Abd al-Uzza]]
* [[Umm Ruman]] 
* [[Asma bint Umais]] 
* [[Family tree of Abu Bakr#Descendants|Habibah bint Kharijah]]}}
| issue = {{plainlist|
* [[Asma bint Abi Bakr|Asma]]
* [[Abd al-Rahman ibn Abi Bakr|Abd al-Rahman]]
* [[Abd Allah ibn Abi Bakr|Abd Allah]]
* [[Aisha]]
* [[Muhammad ibn Abi Bakr|Muhammad]]
* [[Umm Kulthum bint Abi Bakr|Umm Kulthum]]}}
| full name = Abd Allah ibn Abi Quhafa<br/>{{Lang|ar|
عَبْد ٱللَّٰه بْن أَبِي قُحَافَة}}
| father = [[Abu Quhafa]]
| mother = [[Umm al-Khayr]]
{{Infobox|child=yes
| label1 = Brothers
| data1  = {{Plainlist|
* Mu'taq{{efn|Presumably the middle}}
* Utaiq{{efn|Presumably the youngest}}
* Quhafah
}}
| label2 = Sisters
| data2  = 
{{plainlist|
* Fadra
* Qurayba
* Umm Amir
}}
| label3 = Tribe
| data3  = [[Quraysh]] ([[Banu Taym]])
}}
| birth_name = Abd Allah ibn Abi Quhafa
| birth_date = {{circa|573}}
| birth_place = [[Mecca]], [[Hejaz]], [[Pre-Islamic Arabia|Arabia]]
| death_date = {{Death date and age|634|8|23|573|10|27|df=y}} ({{small|22 [[Jumada al-Thani]] 13 [[Hijri year|AH]]}})
| death_place = [[Medina]], Hejaz, [[Rashidun Caliphate]]
| burial_place = [[Al-Masjid an-Nabawi]], Medina
| occupation = Businessman, public administrator, economist
| religion = [[Islam]]
}}

'''Abd Allah ibn Abi Quhafa''' ({{Langx|ar|عَبْدُ اللهِ بْنُ أَبِي قُحَافَةَ|translit=ʿAbd Allāh ibn ʾAbī Quḥāfa|engvar=gb}}) ({{Circa|573}}{{snd}}23 August 634), better known by his ''[[Kunya (Arabic)|kunya]]'' '''Abu Bakr''',{{efn|{{Langx|ar|أبو بكر|translit=ʾAbū Bakr|engvar=gb}}}} was a senior [[Companions of the Prophet|companion]], the closest friend, and father-in-law of [[Muhammad]], the [[Prophets and messengers in Islam|Islamic prophet]]. He served as the first [[Caliphate|caliph]] of the [[Rashidun Caliphate]], ruling from 632 until his death in 634. Abu Bakr was granted the honorific title {{Transliteration|engvar=gb|ar|al-Ṣiddīq}}{{efn|{{Langx|ar|
الصِّدِّيق}}}} (lit. the Veracious or Truthful) by Muhammad, a designation that continues to be used by [[Sunni Muslims]] to this day.

Born to [[Abu Quhafa]] and [[Umm al-Khayr]] of the [[Banu Taym]], Abu Bakr was among the [[Early Muslims|earliest converts]] to [[Islam]] and preached [[dawah]] to the [[Mushrikites|
polytheists]]. He was considered the first [[Da'i|Muslim missionary]], as several companions of Muhammad converted through Abu Bakr. He accompanied Muhammad on his [[Hijrah|migration to Medina]] and became one of his [[Haras (unit)|bodyguards]]. Abu Bakr participated in all of [[List of expeditions of Muhammad|Muhammad's campaigns]] and served as the first {{Transliteration|engvar=gb|ar|[[amir al-hajj]]}} in 631. In the absence of Muhammad, Abu Bakr led the prayers.

Following [[Muhammad#Death|Muhammad's death]] in 632, Abu Bakr [[Succession to Muhammad|succeeded the leadership]] of the [[Muslim]] community as the first caliph, being elected at [[Saqifa]]. His election was contested by a number of rebellious tribal leaders. During his reign, he overcame a number of uprisings, collectively known as the [[Ridda Wars]], as a result of which he was able to consolidate and expand the rule of the Muslim state over the entire [[Arabian Peninsula]]. He also commanded the initial incursions into the neighbouring [[Sasanian Empire|Sasanian]] and [[Byzantine Empire|Byzantine empires]], which in the years following his death, would eventually result in the Muslim [[Muslim conquest of Persia|conquests of Persia]] and [[Muslim conquest of Syria|the Levant]]. Apart from politics, Abu Bakr is also credited for the compilation of the [[Quran]], of which he had a personal caliphal codex. Prior to dying in August 634, Abu Bakr nominated [[Umar]] ({{Reign|634|644}}) as his successor. Along with Muhammad, Abu Bakr is buried in the [[Green Dome]] at the [[Prophet's Mosque|Al-Masjid al-Nabawi]] in [[Medina]], the [[Holiest sites in Islam|second holiest site in Islam]]. He died of illness after a reign of 2 years, 2 months and 14 days, the only Rashidun caliph to die of natural causes.

Though Abu Bakr's reign was brief, it included successful invasions of the two most powerful empires of the time, the [[Sasanian Empire|Sasanian]] and [[Byzantine Empire|Byzantine empires]]. He set in motion a historical trajectory that, within a few decades, would lead to the establishment of one of the largest empires in history. His decisive victory over the local Arab rebel forces marks a significant chapter in [[History of Islam|Islamic history]]. [[Sunni Islam|Sunni tradition]] reveres Abu Bakr as the first of the [[Rashidun|Rashidun caliphs]] and the greatest individual after the [[Prophets and messengers in Islam|prophets and messengers]], while [[Twelver Shi'ism|Twelver Shia]] tradition views Abu Bakr as a usurper of the caliphate and an adversary of the {{Transliteration|engvar=gb|ar|[[ahl al-bayt]]}}.

== Lineage and titles ==
{{Main|Family tree of Abu Bakr}}

[[File:A map of the descendants of Abu Bakr al-Siddiq.svg | thumb | right | alt=A map showing the descendants of Abu Bakr al-Siddiq from his three sons: ʿAbdullāh, ʿAbdur-Raḥmān, and Muḥammad, and their progeny.
 | This is a map of the descendants of Abu Bakr al-Siddiq, showing the classification of each generation (Companion, Successor).
]]
According to [[Ibn Sa'd]], Abu Bakr's full name was Abdullah ibn [[Uthman Abu Quhafa|Abi Quhafa]] ibn Amer ibn Amr ibn Ka'ab ibn Sa'ad ibn [[Banu Taym|Taym]] ibn [[Murrah ibn Ka'b|Murrah]] ibn [[Ka'b ibn Lu'ayy|Ka'b]] ibn [[Lu'ayy ibn Ghalib|Lu'ayy]] ibn [[Ghalib ibn Fihr|Ghalib]] ibn [[Fihr ibn Malik|Fihr]] ibn [[Malik ibn al-Nadr|Malik]] ibn [[Al-Nadr ibn Kinanah|Al-Nader]] ibn [[Kinanah ibn Khuzayma|Kinanah]] ibn [[Khuzayma ibn Mudrika|Khouzayma]] ibn [[Mudrikah ibn Ilyas|Mudrikah]] ibn [[Ilyas ibn Mudar|Ilyas]] ibn [[Mudar ibn Nizar|Mudhar]] ibn [[Nizar ibn Ma'add|Nizar]] ibn [[Ma'add]] ibn [[Adnanites|Adnan]].<ref>[[Ibn Sa'd#Kitāb al-Ṭabaqāt al-Kabīr|Tabaqat ibn Sa'd]] 3/ 169</ref> This lineage meets the lineage of Muhammad at the sixth generation with Murrah ibn Ka'b.

=== Abdullah ===
In [[Arabic]], the name ''Abd Allah'' means "servant of [[Allah]]". This is his birth name.

=== Abu Bakr ===
The nicknames ([[Kunya (Arabic)|kunya]]) literal meaning of "Abu Bakr" is "father of youth," or "father of the firstborn," derived from the core word Bakr, meaning "youth," or "young one."<ref>{{Citation |title=بكر |date=2025-10-27 |work=Wiktionary, the free dictionary |url=https://en.wiktionary.org/w/index.php?title=%D8%A8%D9%83%D8%B1&oldid=87618431 |access-date=2025-11-01 |language=en}}</ref><ref>{{Cite web |last=www.wisdomlib.org |date=2025-07-31 |title=Meaning of the name Bakr |url=https://www.wisdomlib.org/names/bakr |access-date=2025-11-01 |website=www.wisdomlib.org |language=en}}</ref>

It is said that this name was given to him as a child when he grew up among his Bedouin tribe and developed a fondness for camels. He played with the camel calves and goats, earning this nickname "Abu Bakr", meaning "father of the young (camel)." A "bakr" in Arabic is generally used to refer to a young, but already grown camel.

=== Ateeq ===
One of his early titles, preceding his conversion to Islam, was ''Ateeq'', meaning "saved one". In a weak narration in Tirmidhi,<ref name="ateeqhadith">{{cite web |title=Jami' at-Tirmidhi 3679 - Chapters on Virtues - كتاب المناقب عن رسول الله صلى الله عليه وسلم - Sunnah.com - Sayings and Teachings of Prophet Muhammad (صلى الله عليه و سلم) |url=https://sunnah.com/tirmidhi:3679 |website=sunnah.com |access-date=2 October 2023}}</ref> Muhammad later restated this title when he said that Abu Bakr is the "Ateeq of Allah from the fire" meaning "saved" or "secure" and the association with Allah showing how close to and protected he is by Allah.<ref>Abi Na'eem, "Ma'arifat al-sahaba", no. 60</ref>

=== al-Siddiq ===
He was called {{Transliteration|engvar=gb|ar|al-Ṣiddīq}} ("the truthful")<ref name="Campo2">{{Cite book |last=Campo |first=Juan Eduardo |url=https://books.google.com/books?id=OZbyz_Hr-eIC&pg=PP1 |title=Encyclopedia of Islam |date=15 April 2009 |publisher=[[Infobase|Infobase Publishing]] |isbn=9781438126968 |via=Google Books}}</ref> by Muhammad after he believed him in the event of [[Isra' and Mi'raj|Isra and Mi'raj]] when many people did not, and [[Ali]] confirmed that title several times.<ref>Abi Na'eem, "Ma'arifat al-sahaba", no. 64, 65</ref> He was also referred to in the Quran as the "second of the two in the cave" in reference to the event of [[Hijrah|Hijra]], where with Muhammad he hid in the cave in [[Jabal Thawr]] from the [[Mecca|Meccans]] sent after them.<ref>{{Cite book |url=https://books.google.com/books?id=focLrox-frUC&pg=PP1 |title=The New Encyclopedia of Islam |first=Cyril |last=Glassé |date=15 April 2003 |publisher=[[Rowman & Littlefield|Rowman Altamira]] |isbn=9780759101906 |via=Google Books}}</ref>
While traditional sources translate this epithet as "the truthful", an equally likely interpretation is "the tax collector" (i.e., the collector of {{Transliteration|engvar=gb|ar|[[Sadaqah|ṣadaqa]]}}).<ref>{{Cite book |last=Donner |first=Fred M. |author-link=Fred Donner |url=https://books.google.com/books?id=qBzRj7OajmEC |title=Muhammad and the Believers - At the Origins of Islam |date=2010 |publisher=[[Harvard University Press]] |isbn=978-0-674-05097-6 |page=102 |language=en}}</ref>

=== al-Sahib ===
He was honorifically called "al-sahib" (the companion) in the Qur'an, describing his role as a companion of Muhammad when hiding from the [[Quraysh]] in the [[Jabal Thawr]] cave during the [[Hijrah|Hijra]] to [[Medina]]:<ref name="sallaby1">{{cite book |last1=محمد الصلابي |first1=علي |title=سيرة أبي بكر الصديق شخصيته وعصره |url=https://www.noor-book.com/%D9%83%D8%AA%D8%A7%D8%A8-%D8%B3%D9%8A%D8%B1%D8%A9-%D8%A3%D8%A8%D9%8A-%D8%A8%D9%83%D8%B1-%D8%A7%D9%84%D8%B5%D8%AF%D9%8A%D9%82-%D8%B4%D8%AE%D8%B5%D9%8A%D8%AA%D9%87-%D9%88%D8%B9%D8%B5%D8%B1%D9%87-pdf |access-date=2 October 2023}}</ref>
{{blockquote|˹It does not matter˺ if you ˹believers˺ do not support him, for Allah did in fact support him when the disbelievers drove him out ˹of Mecca˺ and he was only one of two. While they both were in the cave, he reassured '''his companion''', "Do not worry; Allah is certainly with us". So Allah sent down His serenity upon the Prophet, supported him with forces you ˹believers˺ did not see, and made the word of the disbelievers lowest, while the Word of Allah is supreme. And Allah is Almighty, All-Wise.{{qref|9|40|s=y|t=c}}}}

=== Al-Atqā ===
In a [[hadith]] narrated by Ibn Abbas of the exegesis of [[Al-Lail|chapter 92 of the Qur'an]] by imam [[Al-Suyuti]], we find the word "al-atqā" ({{langx|ar|الأتقى}}), meaning "the most pious", "the most righteous", or "the most God-fearing", is referring to Abu Bakr as an example for the believers.<ref name="sallaby1">{{cite book |last1=محمد الصلابي |first1=علي |title=سيرة أبي بكر الصديق شخصيته وعصره |url=https://www.noor-book.com/%D9%83%D8%AA%D8%A7%D8%A8-%D8%B3%D9%8A%D8%B1%D8%A9-%D8%A3%D8%A8%D9%8A-%D8%A8%D9%83%D8%B1-%D8%A7%D9%84%D8%B5%D8%AF%D9%8A%D9%82-%D8%B4%D8%AE%D8%B5%D9%8A%D8%AA%D9%87-%D9%88%D8%B9%D8%B5%D8%B1%D9%87-pdf |access-date=2 October 2023}}</ref><ref name="alduralmanthoor">{{cite web |last1=Al-Suyuti |first1=Jalaladin |title=al-Dur al-Manthoor |url=https://tafsir.app/aldur-almanthoor/92/17 |website=tafsir.app |access-date=2 October 2023}}</ref>

{{blockquote|And so I have warned you of a raging Fire, in which none will burn except the most wretched—who deny and turn away. But '''the righteous''' will be spared from it – who donate ˹some of˺ their wealth only to purify themselves, not in return for someone's favours, but seeking the pleasure of their Lord, the Most High. They will certainly be pleased.{{qref|92|14-21|s=y|t=c}}}}

=== Al-Awwāh ===
"Al-Awwāh" ({{langx|ar|الأواه}}) means someone who supplicates abundantly to God, someone who is merciful and the gentle-hearted. [[Ibrahim al-Nakha'i]] said that Abu Bakr has also been called al-awwāh for his merciful character.<ref name="sallaby1" />

== Early life ==
Abu Bakr was born in [[Mecca]] sometime in 573 to a rich family in the Banu Taym tribe of the Quraysh tribal confederacy.<ref>{{Cite book |title=Islamic Thought: From Mohammed to 11 September 2001 |last=Al-Jubouri |first=I.M.N. |date=2010 |page=53 |publisher=Xlibris Corporation |isbn=9781453595855 |url=https://books.google.com/books?id=owqY-90imMIC&pg=PA53}}</ref> His father was [[Abu Quhafa]] and his mother was [[Umm al-Khayr]].<ref name="Saritoprak">{{cite web |last1=Saritoprak |first1=Zeki |title=Abu Bakr Al-Siddiq |url=http://www.oxfordbibliographies.com/view/document/obo-9780195390155/obo-9780195390155-0221.xml |access-date=12 December 2018 |website=oxfordbibliographies.com |publisher=[[Oxford University Press]]}}</ref>

He spent his early childhood like other Arab children of the time, among the [[Bedouin]]s who called themselves ''Ahl-i-Ba'eer'' (the people of the camel) and developed a particular fondness for camels. In his early years, he played with the camel calves and goats, and his love for camels earned him the nickname (''[[Kunya (Arabic)|kunya]]'') "''Abu Bakr''", the father of the camel's calf.<ref>{{Cite book |title=Islam for Nerds – 500 Questions and Answers |last=Drissner |first=Gerald |publisher=createspace |date=2016 |isbn=978-1530860180 |page=432}}</ref><ref>''War and Peace in the Law of Islam'' by [[Majid Khadduri]], translated by Muhammad Yaqub Khan Published 1951 Ahmadiyyah Anjuman Ishaat Islam, original from the [[University of Michigan]], digitised 23 October 2006</ref>

Like other children of the rich Meccan merchant families, Abu Bakr was literate and developed a fondness for [[Arabic poetry|poetry]]. He used to attend the annual fair at [[Souk Okaz|Ukaz]], and participate in poetical symposia. He had a very good memory and a good knowledge of the [[genealogy]] of the Arab tribes, their stories and their politics.<ref>Al-Zarkali, ''Al-A'lam'', Dar al-Ilm lil Malayeen, 15th edition, May 2002</ref>

A story is preserved that once when he was a child, his father took him to the [[Kaaba]] and asked him to pray before the [[Idolatry|idols]]. His father went away to attend to some other business, and Abu Bakr was left alone. Addressing an idol, Abu Bakr said, "O my God, I am in need of beautiful clothes; bestow them on me". The idol remained indifferent. Then he addressed another idol, saying, "O God, give me some delicious food. See that I am so hungry". The idol remained cold. That exhausted the patience of young Abu Bakr. He lifted a stone, and, addressing an idol, said, "Here I am aiming a stone; if you are a god protect yourself". Abu Bakr hurled the stone at the idol and left the Kaaba.<ref>{{cite book |title=Sidiq-i-Akbar Hazrat Abu Bakr |author=Masud-ul-Hasan |page=2 |publisher=[[Ferozsons]]}}</ref> Regardless, it recorded that prior to reverting to Islam, Abu Bakr practiced as a ''[[hanif]]'' and never worshipped idols.<ref>{{Cite news |url=http://www.oxfordbibliographies.com/view/document/obo-9780195390155/obo-9780195390155-0221.xml |title=Abu Bakr Al-Siddiq – Islamic Studies |publisher=[[Oxford Bibliographies Online]] |access-date=13 September 2018}}</ref>

== Companionship of Muhammad ==
[[File:New York Public Library, Spencer Collection Turk. MS. 3 Siyar-i Nabî fol. 136r Abû Lahab and his followers hurl stones at Muhammad and Abû Bakr at the 'Ukâz Fair.jpg|thumb|245x245px|A 1595 [[Ottoman Empire|Ottoman]] [[Miniature art|miniature]] from the ''[[Siyer-i Nebi]]'' depicting Abu Bakr intervening to stop a Meccan mob, led by Abu Lahab, from attacking Muhammad at the [[Souk Okaz|Souk of Okaz]].]]
While some Sunni scholars and all [[Shi'a]] traditions maintain that [[Ali ibn Abi Talib]] was the second person to embrace Islam after [[Khadija bint Khuwaylid|Khadija]], the historian [[Ibn Kathir]], in ''[[Al-Bidaya wa'l-Nihaya]]'', disregards this view and classifies the earliest converts by social group: Khadija as the first woman, [[Zayd ibn Harithah]] as the first freed slave, Ali ibn Abi Talib as the first child, and Abu Bakr as the first free adult man to embrace Islam.<ref name="archive.org">[https://archive.org/stream/TheBiographyOfAbuBakrAsSiddeeq/TheBiographyOfAbuBakrAs-siddeeq#page/n53/mode/2up The Biography Of Abu Bakr As Siddeeq] by Dr. Ali Muhammad As-Sallaabee (Published 2007)</ref><ref name="brit">{{Cite web |date=19 August 2023 |title=Abu Bakr - Biography & Facts |url=https://www.britannica.com/biography/Abu-Bakr |website=britannica.com}}</ref><ref name="Saritoprak" /><ref name="Campo2">{{Cite book |last=Campo |first=Juan Eduardo |url=https://books.google.com/books?id=OZbyz_Hr-eIC&pg=PP1 |title=Encyclopedia of Islam |date=15 April 2009 |publisher=[[Infobase|Infobase Publishing]] |isbn=9781438126968 |via=Google Books}}</ref>

=== Subsequent life in Mecca ===
His wife [[Qutaylah bint Abd al-Uzza]] did not accept Islam and he divorced her. His other wife, [[Umm Ruman]], became a Muslim. All his children accepted Islam except [[Abd al-Rahman ibn Abi Bakr|Abd al-Rahman]], from whom Abu Bakr disassociated himself. His conversion also brought many people to Islam. He persuaded his intimate friends to revert,<ref>[[Al-Bidaya wa l-Nihaya]] 3/26</ref><ref>[[Merriam-Webster]]'s ''Encyclopedia of World Religions'' by Wendy Doniger {{ISBN|978-0-87779-044-0}}</ref> and presented Islam to other friends in such a way that many of them also accepted the faith. Those who reverted to Islam at the dawah of Abu Bakr were:<ref name="Ashraf">{{cite book |last1=Ashraf |first1=Shahid |title=Encyclopaedia Of Holy Prophet And Companion (Set Of 15 Vols.) |date=2004 |publisher=Anmol Publications Pvt. Limited |isbn=978-81-261-1940-0 |url=https://books.google.com/books?id=QWqctAEACAAJ |language=en}}</ref>
* [[Uthman|Uthman ibn Affan]] (who would become the 3rd Caliph)
* [[Zubayr ibn al-Awwam]] (who played a part in the [[Muslim conquest of Egypt]])
* [[Talha ibn Ubayd Allah]], his cousin and an important companion of Muhammad.
* [[Abd al-Rahman ibn Awf]] (who would remain an important part of the [[Rashidun Caliphate]])
* [[Sa'd ibn Abi Waqqas]] (who played a leading role in the [[Muslim conquest of Persia|Islamic conquest of Persia]])
* [[Abu Ubayda ibn al-Jarrah]] (who was a commander in chief of the [[Rashidun army]] in Levant)
* [[Abu Salama]] was a foster brother of Muhammad.
* [[Khalid ibn Sa'id]], who acted as a general under the Rashidun army in Syria

Abu Bakr's acceptance proved to be a milestone in Muhammad's mission. Slavery was common in Mecca, and many slaves accepted Islam. When an ordinary free man accepted Islam, despite opposition, he would enjoy the protection of his tribe. For slaves, however, there was no such protection and they commonly experienced persecution. Abu Bakr felt compassion for slaves, so he purchased eight (four men and four women) and then freed them, paying 40,000 [[dinar]] for their freedom.<ref>[[The Book of the Major Classes|Tabaqat ibn Sa'd]] 3/ 169, 174</ref><ref>[[History of the Prophets and Kings|Tarikh ar-Rusul wa al-Muluk]] 3/ 426</ref> He was known to have freed slaves, including [[Bilal ibn Rabah]], who later became the first [[Muezzin]].

The men were:
* [[Bilal ibn Rabah]]
* [[Abu Fukayha]]
* [[Ammar ibn Yasir]]
* [[Amir ibn Fuhayra]]

The women were:
* [[Lubaynah]]
* [[Al-Nahdiah]]
* [[Umm Ubays]]
* [[Zunairah al-Rumiya|Harithah bint al-Muammil]]

Most of the slaves liberated by Abu Bakr were either women or old and frail men.<ref>''The Mohammedan Dynasties: Chronological and Genealogical Tables with Historical Introductions (1894)'' by [[Stanley Lane-Poole]], published by Adamant Media Corporation {{ISBN|978-1-4021-6666-2}}</ref> When his father asked him why he did not liberate strong and young slaves, who could be a source of strength for him, Abu Bakr replied that he was freeing the slaves for the sake of [[God in Islam|God]], and not for his own sake.

=== Persecution by the Quraysh, 613 ===
{{Main|Persecution of Muslims by the Meccans}}

For three years after the birth of Islam, Muslims kept their faith private. In 613, according to Islamic tradition, Muhammad was commanded by God to call people to Islam openly. The first public address inviting people to offer allegiance to Muhammad was delivered by Abu Bakr.<ref>Muslim persecution of heretics during the marwanid period (64-132/684-750), Judd Steven, ''Al-Masq: Islam & the Medieval Mediterranean'', April 2011, Vol. 23, Issue 1, pp. 1–14.</ref> In a fit of fury, the young men of the [[Quraysh]] tribe rushed at Abu Bakr and beat him until he lost consciousness.<ref>''Abu Bakr'' by Atta Mohy-ud-Din, published 1968 S. Chand Original from the University of Michigan, digitised 6 January 2006, [[ASIN]] B0006FFA0O.</ref> Following this incident, his mother reverted to Islam. Abu Bakr was persecuted many times by the Quraysh. Though Abu Bakr's beliefs would have been defended by his own clan, it would not be so for the entire Quraysh tribe.

=== Last years in Mecca ===
In 617, the Quraysh enforced a boycott against the [[Banu Hashim]]. Muhammad, along with his supporters from Banu Hashim, were cut off in a pass away from [[Mecca]]. All social relations with the Banu Hashim were cut off and their state was that of [[imprisonment]].<ref>{{Cite web |date=10 November 2013 |title=The Economic and Social Boycott of the Banu Hashim |url=https://www.al-islam.org/restatement-history-islam-and-muslims-sayyid-ali-asghar-razwy/economic-and-social-boycott-banu |access-date=22 June 2024 |website=al-islam.org |language=en}}</ref> Before it, many Muslims migrated to [[Abyssinia]] (now [[Ethiopia]] and [[Eritrea]]). Abu Bakr, feeling distressed, set out for Yemen and then to Abyssinia from there. He met a friend of his, Ad-Dughna (chief of the Qarah tribe) outside Mecca, who invited Abu Bakr to seek his protection against the Qurayshites. Abu Bakr went back to Mecca, which was a relief for him. But soon due to the pressure of the Quraysh, Ad-Dughna was forced to renounce his protection. Once again the Quraysh were free to persecute Abu Bakr.

In 620, Muhammad's uncle and protector, [[Abu Talib ibn Abd al-Muttalib]], and Muhammad's wife [[Khadija bint Khuwaylid|Khadija]] died. Abu Bakr's daughter [[Aisha]] was betrothed to Muhammad; however, it was decided that the actual marriage ceremony would be held later. In 620, Abu Bakr was the first person to testify to Muhammad's [[Isra' and Mi'raj|Isra and Mi'raj]].<ref>''Islam (Exploring Religions)'' by Anne Geldart, published by [[Heinemann (book publisher)|Heinemann Library]], 28 September 2000 {{ISBN|978-0-431-09301-7}}</ref>

=== Migration to Medina ===
{{Main|Hijra (Islam)}}

[[File:New York Public Library, Spencer Collection Turk. MS. 3 Siyar-i Nabî fol.296r Muhammad and Abu Bakr in Thawr cave.jpg|left|thumb|Muhammad (left) and Abu Bakr hiding in the cave in [[Jabal Thawr]] from ''[[Siyer-i Nebi]]''.]]
In 622, on the invitation of the Muslims of Yathrib (later [[Medina]]), Muhammad ordered his followers to migrate there. The migration began in batches. Ali was the last to remain in Mecca, entrusted with responsibility for settling any loans the Muslims had taken out, and famously slept in the bed of Muhammad when the Quraysh, led by [[Ikrima ibn Amr|Ikrima]], attempted to murder Muhammad as he slept. Meanwhile, Abu Bakr accompanied Muhammad to Medina. Due to the danger posed by the Quraysh, they did not take the road, but moved in the opposite direction, taking refuge in a cave in [[Jabal Thawr]], some five miles south of Mecca. [[Abd Allah ibn Abi Bakr]], the son of Abu Bakr, would listen to the plans and discussions of the Quraysh, and at night, he would carry the news to the fugitives in the cave. [[Asma bint Abi Bakr]], the daughter of Abu Bakr, brought them meals every day.<ref>''Islamic Culture'' by the Islamic Cultural Board Published 1927 s.n. Original from the University of Michigan, digitised 27 March 2006.</ref> Amir, a servant of Abu Bakr, would bring a flock of goats to the mouth of the cave every night, where they were milked. The Quraysh sent search parties in all directions. One party came close to the entrance to the cave but was unable to see them. Abu Bakr was referenced in the [[Qur'an]] in verse 40 of {{Transliteration|engvar=gb|ar|[[Surah|sura]]}} {{Transliteration|engvar=gb|ar|[[at-Tawba]]}}: {{quote|"If ye help him not, still God helped him when those who disbelieve drove him forth, the {{Transliteration|engvar=gb|ar|thaniya ithnayn}} (second of the two i.e. Abu Bakr); when they two were in the cave, when he said unto his {{Transliteration|engvar=gb|ar|sahib}} (companion i.e. Abu Bakr): Grieve not. Lo! Allah is with us."<ref>{{Cite web |title=Surah Taubah Ayat 40 (9:40 Quran) With Tafsir |url=https://myislam.org/surah-taubah/ayat-40/ |access-date=6 February 2024 |website=myislam.org |language=en}}</ref>}}

=== Life in Medina ===
Kharijah ibn Zaid al-Ansari lived at Sunh, a suburb of Medina, and Abu Bakr also settled there. After Abu Bakr's family arrived in Medina, he bought another house near Muhammad's.<ref>[[Hazrat]] ''Abu Bakr, the First Caliph of Islam'' by Muhammad Habibur Rahman Khan Sherwani, published 1963, Sh. Muhammad Ashraf, original from the [[University of Michigan]], digitised 14 November 2006.</ref> While the climate of Mecca was dry, the climate of Medina was damp, and because of this, most of the migrants fell sick on arrival. Abu Bakr contracted a fever for several days, during which time he was attended to by Kharijah and his family. In Mecca, Abu Bakr was a wholesale trader in cloth and he started the same business in Medina. He opened his new store at Sunh, and from there cloth was supplied to the market at Medina. Soon his business flourished. Early in 623, Abu Bakr's daughter Aisha, who was already married to Muhammad, was sent on to Muhammad's house after a simple marriage ceremony, further strengthening relations between Abu Bakr and Muhammad.<ref name="Maghazi">Tabqat ibn al-Saad book of Maghazi, p. 62</ref>

=== Military campaigns under Muhammad ===
{{Sunni Islam|Rightly Guided Caliphs}}

=== Battle of Badr ===
{{Main|Battle of Badr}}

In 624, Abu Bakr was involved in the first battle between the Muslims and the Quraysh of Mecca, known as the [[Battle of Badr]], but likely did not fight in the main battlefield itself, instead acting as one of the guards of Muhammad's tent. In relation to this, Ali later asked his associates as to who they thought was the bravest among men. Everyone stated that Ali was the bravest of all men. Ali then replied:

{{blockquote|No. Abu Bakr is the bravest of men. In the Battle of Badr we had prepared a pavillion for the prophet, but when we were asked to offer ourselves for the task of guarding it none came forward except Abu Bakr. With a drawn sword he took his stand by the side of Prophet of Allah and guarded him from the infidels by attacking those who dared to proceed in that direction. He was therefore the bravest of men.<ref>Sidiq-i-Akbar Hazrat Abu Bakr by Prof. Masud-ul-Hasan, p. 31, printed and published by A. Salam, [[Ferozsons]], 60, Shahrah-e-Quaid-e-Azam, Lahore</ref>}}

In Sunni accounts, during one such attack, two discs from Abu Bakr's shield penetrated into Muhammad's cheeks. Abu Bakr went forward with the intention of extracting these discs but [[Abu Ubayda ibn al-Jarrah]] requested he leave the matter to him, losing his two incisors during the process. In these stories subsequently Abu Bakr, along with other companions, led Muhammad to a place of safety.<ref name="Maghazi"/>

=== Battle of Uhud ===
{{Main|Battle of Uhud}}

In 625, he participated in the [[Battle of Uhud]], in which the majority of the Muslims were routed and he himself was wounded.<ref>{{cite book |last=Morgan |first=Diane |title=Essential Islam: A Comprehensive Guide to Belief and Practice |publisher=[[ABC-Clio]] |date=2010 |page=126 |isbn=9780313360268 |url=https://archive.org/details/essentialislamco0000morg |url-access=registration}}</ref> Before the battle began, his son [[Abd al-Rahman ibn Abi Bakr|Abd al-Rahman]], at that time still non-Muslim and fighting on the side of the Quraysh, came forward and threw down a challenge for a duel. Abu Bakr accepted the challenge but was stopped by Muhammad.<ref>{{cite book |last=Sherwani |first=Muhammad Habibur Rahman Khan |title=Hazrat Abu Bakr, the First Caliph of Islam |date=1963 |page=23}}</ref> In the second phase of the battle, [[Khalid ibn al-Walid]]'s cavalry attacked the Muslims from behind, changing a Muslim victory to defeat.<ref>{{cite book |author=Watt, W. Montgomery |author-link=W. Montgomery Watt |title=Muhammad: Prophet and Statesman |publisher=[[Oxford University Press]] |date=1974 |isbn=0-19-881078-4 |pages=138–139 |url=https://archive.org/details/muhammadprophets00watt/page/138 }}</ref><ref>"Uhud", ''Encyclopedia of Islam Online''</ref>

=== Battle of the Trench ===
{{Main|Battle of the Trench}}

In 627, he participated in the [[Battle of the Trench]] and also in the [[Siege of Banu Qurayza]].<ref name="Maghazi"/> In the Battle of the Trench, Muhammad divided the ditch into a number of sectors and a contingent was posted to guard each sector. One of these contingents was under the command of Abu Bakr. The enemy made frequent assaults in an attempt to cross the ditch, all of which were repulsed. To commemorate this event, a [[mosque]], later known as 'Masjid-i-Siddiq',<ref>{{cite book |title=Sidiq-i-Akbar Hazrat Abu Bakr |author=Masud-ul-Hasan |page=36 |publisher=[[Ferozsons]]}}</ref> was constructed at the site where Abu Bakr repulsed the charges of the enemy.<ref name="Maghazi"/>

=== Battle of Khaybar ===
{{Main|Battle of Khaybar}}

Abu Bakr took part in the [[Battle of Khaybar]]. Khaybar had eight fortresses, the strongest and most well-guarded of which was called Al-Qamus. Muhammad sent Abu Bakr with a group of warriors to attempt to take it, but they were unable to do so. Muhammad also sent Umar with a group of warriors, but Umar could not conquer Al-Qamus either.<ref>{{cite book |last1=Razwy |first1=Sayed Ali Asgher |title=A Restatement of the History of Islam & Muslims |page=192}}</ref><ref name=":0" /><ref name="The Life of Mohammed">{{cite book |last1=Irving |first1=Washington |title=The Life of Mohammed}}</ref><ref>{{cite book |last1=Haykal |first1=Muhammad Husayn |title=The Life of Muhammad |date=1935 |quote=As the days went by, the Prophet sent Abu Bakr with a contingent and a flag to the fortress of Na'im; but he was not able to conquer it despite heavy fighting. The Prophet then sent Umar bin al-Khattab on the following day, but he fared no better than Abu Bakr.}}</ref> Some other Muslims also attempted to capture the fort, but they were unsuccessful as well.<ref>{{cite book |last1=Razwy |first1=Sayed Ali Asgher |title=A Restatement of the History of Islam & Muslims |pages=192–193 |quote=Some other captains also tried to capture the fortress but they also failed.}}</ref> Finally, Muhammad sent Ali, who defeated the enemy leader, [[Marhab]].<ref name="The Life of Mohammed"/><ref>{{cite book |last1=Razwy |first1=Sayed Ali Asgher |title=A Restatement of the History of Islam & Muslims |page=193}}</ref>

=== Military campaigns during final years of Muhammad ===
{{Main|Expedition of Amr ibn al-As|Conquest of Mecca|Expedition of Tabuk}}

In 629, Muhammad sent [[Amr ibn al-As]] to Zaat-ul-Sallasal, followed by [[Abu Ubayda ibn al-Jarrah]] in response to a call for reinforcements. Abu Bakr and Umar commanded an army under al-Jarrah. They attacked and defeated the enemy.<ref>Sahih-al-Bhukari book of Maghazi, Ghazwa Saif-al-Jara</ref> In 630, when [[Conquest of Mecca|the Muslims conquered Mecca]], Abu Bakr was part of the army.<ref>{{Cite web |last=Lasani |first=Yousaf Manzoor |date=12 July 2020 |title=Who was Hazrat Abu Bakr (RA)? His Life and Contributions to Islam |url=https://zillnoorain.com/who-was-hazrat-abu-bakr-ra-his-life-and-contributions-to-islam/ |access-date=23 June 2024 |website=zillnoorain.com |language=en |archive-date=23 June 2024 |archive-url=https://web.archive.org/web/20240623144234/https://zillnoorain.com/who-was-hazrat-abu-bakr-ra-his-life-and-contributions-to-islam/ |url-status=dead }}</ref> Before the conquest, his father reverted to Islam.<ref>{{Cite web |last=slife |date=26 December 2018 |title=Conquest of Mecca |url=https://slife.org/conquest-of-mecca/ |access-date=22 June 2024 |website=The Spiritual Life |language=en}}</ref><ref>{{Cite web |date=28 November 2022 |title=Facts about Abu Bakr al-Siddiq |url=https://www.studioarabiyainegypt.com/facts-about-abu-bakr-al-siddiq/ |access-date=23 June 2024 |website=studioarabiyainegypt.com |language=en}}</ref>

==== Battles of Hunayn and Ta'if ====
{{Main|Battle of Hunayn|Siege of Ta'if}}

In 630, the Muslim army was [[Battle of Hunayn|ambushed by archers from the local tribes]] as it passed through the valley of [[Hunayn, Saudi Arabia|Hunayn]], around eleven miles northeast of Mecca. Surprised, the advance guard of the Muslim army fled in panic. There was considerable confusion, and the camels, horses and men ran into one another in an attempt to seek cover. Muhammad, however, stood firm. Only nine companions remained around him, including Abu Bakr. Under Muhammad's instruction, his uncle [[Abbas ibn Abd al-Muttalib|Abbas]] shouted at the top of his voice, "O Muslims, come to the Prophet of Allah". The call was heard by the Muslim soldiers and they gathered beside Muhammad. When the Muslims had gathered in sufficient number, Muhammad ordered a charge against the enemy. In the hand-to-hand fight that followed the tribes were routed and fled to [[Awtas|Autas]].

Muhammad posted a contingent to guard the Hunayn pass and led the main army to Autas. In the confrontation at Autas, the tribes could not withstand the Muslim onslaught. Believing that continued resistance would be useless, the tribes broke camp and retired to [[Taif|Ta'if]]. Abu Bakr was commissioned by Muhammad to lead the [[Siege of Ta'if|attack against Ta'if]]. The tribes shut themselves in the fort and refused to come out in the open. The Muslims employed catapults, but without tangible result. The Muslims attempted to use a [[testudo formation]], in which a group of soldiers shielded by a cover of cowhide advanced to set fire to the gate. However, the enemy threw red hot scraps of iron on the testudo, rendering it ineffective.

The siege dragged on for two weeks, and there was still no sign of weakness in the fort. Muhammad held a council of war. Abu Bakr advised that the siege might be raised and that God make arrangements for the fall of the fort. The advice was accepted, and in December 630, the siege of Ta'if was raised and the Muslim army returned to [[Mecca]]. A few days later, [[Malik ibn Awf|Malik bin Awf]], the commander, came to Mecca and became a Muslim.<ref>{{cite book |title=Sidiq-i-Akbar Hazrat Abu Bakr |author=Masud-ul-Hasan |page=46 |publisher=[[Ferozsons]]}}</ref>

=== Abu Bakr as Amir al-Hajj ===
In 630–631 (AH 9), Muhammad assigned Abu Bakr as the {{Transliteration|engvar=gb|ar|[[amir al-hajj]]}} to lead around 300 pilgrims from Medina to Mecca.{{sfn|Hathaway|2015}} In 631 AD, Muhammad sent from Medina a delegation of three hundred Muslims to perform the [[Hajj]] according to the new Islamic way and appointed Abu Bakr as the leader of the delegation. The day after Abu Bakr and his party left for the Hajj, Muhammad received a new revelation: Surah [[At-Tawbah|Tawbah]], the ninth chapter of the Quran.<ref>{{cite book |last1=Razwy |first1=Sayed Ali Asgher |title=A Restatement of the History of Islam & Muslims |page=255}}</ref> It is related that when this revelation came, someone suggested to Muhammad that he should send news of it to Abu Bakr. Muhammad said that only a man of his house could proclaim the revelation.<ref name=":0">{{cite book |last1=[[ibn Ishaq]] |first1=Muhammad |title=The Life of the Messenger of God}}</ref>

According to Shia sources, Muhammad summoned Ali and asked him to proclaim a portion of Surah Tawbah to the people on the day of sacrifice when they assembled at [[Mina, Saudi Arabia|Mina]]. Ali went forth on Muhammad's slit-eared camel and overtook Abu Bakr. When Ali joined the party, Abu Bakr wanted to know whether he had come to give orders or to convey them. Ali said that he had not come to replace Abu Bakr as Amir Al-Hajj and that his only mission was to convey a special message to the people on behalf of Muhammad.<ref>{{Cite web |date=2013-11-10 |title=The Proclamation of Surah Bara'ah or Al Tawbah |url=https://al-islam.org/restatement-history-islam-and-muslims-sayyid-ali-asghar-razwy/proclamation-surah-baraah-or-al-tawbah |access-date=2025-04-25 |website=al-islam.org |language=en}}</ref>

At Mecca, Abu Bakr presided at the Hajj ceremony, and Ali read the proclamation on behalf of Muhammad. The main points of the proclamation were:

#Henceforward the non-Muslims were not to be allowed to visit the [[Kaaba]] or perform the pilgrimage;
#No one should [[Circumambulation|circumambulate]] the Kaaba naked;
#[[Polytheism]] was not to be tolerated. Where the Muslims had any agreement with the polytheists, such agreements would be honoured for the stipulated periods. Where there were no agreements, a grace period of four months was provided, and thereafter no quarter was to be given to the polytheists.

From the day this proclamation was made, a new era dawned, and Islam alone was to be supreme in Arabia.

=== Expedition of Abu Bakr As-Siddiq ===
{{Main|Expedition of Abu Bakr As-Siddiq}}

Abu Bakr led one military expedition, the [[Expedition of Abu Bakr As-Siddiq]],<ref name="books.google.co.uk">{{cite book |url=https://books.google.com/books?id=mZmBkoDa9fcC&pg=PA205 |title=Atlas Al-sīrah Al-Nabawīyah |date=1 January 2004 |publisher=[[Darussalam Publishers]] |isbn=9789960897714 |via=Google Books}}</ref> which took place in [[Najd]], in July 628 (third month 7AH in the [[Islamic calendar]]).<ref name="books.google.co.uk"/> Abu Bakr led a company in Nejd on the order of Muhammad. Many were killed and taken prisoner.<ref>[https://archive.org/details/bub_gb_Feo9AAAAYAAJ/page/n102 The life of Mahomet and history of Islam, Volume 4, By Sir William Muir, p. 83] See bottom of page, notes section</ref> The Sunni Hadith collection ''[[Sunan Abi Dawud|Sunan Abu Dawud]]'' mentions the event.<ref>{{Hadith-usc|usc=yes|abudawud|14|2632}}</ref>

=== Expedition of Usama bin Zayd ===
{{Main|Expedition of Usama bin Zayd}}

In 632, during the final weeks of his life, Muhammad ordered an expedition into Syria to avenge the defeat of the Muslims in the [[Battle of Mu'tah]] some years previously. Leading the campaign was [[Usama ibn Zayd]], whose father, Muhammad's erstwhile adopted son [[Zayd ibn Haritha al-Kalbi|Zayd ibn Harithah]], had been killed in the earlier conflict.<ref>{{Cite book |last=Ahmad |first=Fazl |title=Heroes of Islam Series - Abu Bakr, the first caliph of Islam |date=1961 |page=42}}</ref> No more than twenty years old, inexperienced and untested, Usama's appointment was controversial, becoming especially problematic when veterans such as Abu Bakr, [[Abu Ubayda ibn al-Jarrah]], and [[Sa'd ibn Abi Waqqas]] were placed under his command.<ref name=PowersP27>{{Cite book |last=Powers |first=David S. |title=Muhammad Is Not the Father of Any of Your Men - The Making of the Last Prophet |date=2011 |page=27 |publisher=[[University of Pennsylvania Press]] |isbn=9780812205572 |url=https://books.google.com/books?id=KH2FUBSOQ8kC&pg=PA27}}</ref><ref>{{Cite book |first=Hasan M. |last=Balyuzi |author-link=Hasan M. Balyuzi |title=Muḥammad and the Course of Islám |date=1976 |page=151}}</ref> Nevertheless, the expedition was dispatched, though soon after setting off, news was received of Muhammad's death, forcing the army to return to Medina.<ref name=PowersP27/> The campaign was not reengaged until after Abu Bakr's ascension to the caliphate, at which point he chose to reaffirm Usama's command, which ultimately led to its success.<ref>{{Cite web |date=21 March 2016 |title=The Expedition Of Usama Bin Zayd |url=https://discover-the-truth.com/2016/03/21/the-expedition-of-usama-bin-zayd/ |access-date=22 June 2024 |website=discover-the-truth.com |language=en}}</ref>

=== Death of Muhammad ===
There are a number of traditions regarding Muhammad's final days which have been used to reinforce the idea of the great friendship and trust which is existed between him and Abu Bakr.<ref name="DeathMuhammad26">{{cite web |last=Juferi |first=Mohd Elfie Nieshaem |title=The Death of Muhammad: Poison, Prophethood, and the Misreading of Sources |date=14 January 2026 |url=https://bismikaallahuma.org/polemical-rebuttals/death-of-muhammad/ |website=Bismika Allahuma |language=en |access-date=15 January 2026}}</ref> In one such episode, as Muhammad was nearing death, he found himself unable to lead prayers as he usually would. He instructed Abu Bakr to take his place, ignoring concerns from Aisha that her father was too emotionally delicate for the role. Abu Bakr subsequently took up the position, and when Muhammad entered the prayer hall one morning during [[Fajr (prayer)|Fajr prayers]], Abu Bakr attempted to step back to let him to take up his normal place and lead. Muhammad, however, allowed him to continue. In a related incident, around this time, Muhammad ascended the pulpit and addressed the congregation, saying, "God has given his servant the choice between this world and that which is with God and he has chosen the latter". Abu Bakr, understanding this to mean that Muhammad did not have long to live, responded, "Nay, we and our children will be your ransom". Muhammad consoled his friend and ordered that all the doors leading to [[Prophet's Mosque|the mosque]] be closed aside from that which led from Abu Bakr's house, "for I know no one who is a better friend to me than he".{{sfn|Fitzpatrick|Walker|2014|pp=2–3}}{{NoteTag|Such incidents are used by some Sunnis to justify Abu Bakr's later ascension to the caliphate as they display the regard with which Muhammad held the former. However, several other companions had held similar positions of authority and trust, including the leading of prayers. Such honours may therefore not hold much importance in matters of succession.<ref>{{cite book |first=M.A. |last=Shaban |title=Islamic History - a New Interpretation |date=1971 |page=16 |url= https://archive.org/details/IslamicHistoryANewInterpretationVol.1 }}</ref>}}

Upon Muhammad's death, the Muslim community was unprepared for the loss of its leader and many experienced a profound shock. Umar was particularly affected, instead declaring that Muhammad had gone to consult with God and would soon return, threatening anyone who would say that Muhammad was dead.<ref name=PhippsP70>{{cite book |first=William E. |last=Phipps |title=Muhammad and Jesus - A Comparison of the Prophets and Their Teachings |date=2016 |page=70 |publisher=[[Bloomsbury Group|Bloomsbury]] |isbn=9781474289351 |url=https://books.google.com/books?id=DR_mDAAAQBAJ&pg=PA70}}</ref> Abu Bakr, having returned to Medina,<ref>{{cite book |first1=Muzaffar Husain |last1=Syed |first2=Syed Saud |last2=Akhtar |first3=B. D. |last3=Usmani |title=Concise History of Islam |date=2011 |page=27 |publisher=Vij Books India Pvt |isbn=9789382573470 |url=https://books.google.com/books?id=eACqCQAAQBAJ&pg=PA27}}</ref> calmed Umar by showing him Muhammad's body, convincing him of his death.<ref>{{cite book |first=Ingrid |last=Mattson |author-link=Ingrid Mattson |title=The Story of the Qur'an - Its History and Place in Muslim Life |date=2013 |page=185 |publisher=[[Wiley (publisher)|John Wiley & Sons]] |isbn=9780470673492 |url=https://books.google.com/books?id=_-eUnDh_OWgC&pg=PA185}}</ref> He then addressed those who had gathered at the mosque, saying, "If anyone worships Muhammad, Muhammad is dead. If anyone worships God, God is alive, immortal", thus putting an end to any idolising impulse in the population. He then concluded with verses from the Quran: "(O Muhammad) Verily you will die, and they also will die." ({{qref|39|30}}), "Muhammad is no more than an Apostle; and indeed many Apostles have passed away, before him, If he dies or is killed, will you then turn back on your heels? And he who turns back on his heels, not the least harm will he do to Allah and Allah will give reward to those who are grateful." ({{qref|3|144}})<ref>{{Href|bukhari|3667|b=yl}}</ref><ref name=PhippsP70/>

== Caliphate ==

=== Saqifa ===
{{Main|Succession to Muhammad|Saqifa}}

In the immediate aftermath of Muhammad's death, a gathering of the [[Ansar (Islam)|Ansar]] (Natives of Medina) took place in the {{Transliteration|engvar=gb|ar|[[Saqifa]]}} (courtyard) of the [[Banu Sa'ida]] clan.{{sfn|Fitzpatrick|Walker|2014|p=3}}{{sfn|Madelung|1997|pp=30–2}}{{Sfn|Lecomte|2022}} The general belief at the time was that the purpose of the meeting was for the Ansar to decide on a new leader of the [[Ummah|Muslim community]] among themselves, with the intentional exclusion of the [[Muhajirun]] (Immigrants from Mecca), though this has later become the subject of debate.<ref>{{cite book |first=Wilferd |last=Madelung |title=The Succession to Muhammad |date=1997 |page=31 |url=https://archive.org/details/TheSuccessionToMuhammadByWilferdMadelung}}</ref>

Nevertheless, Abu Bakr and Umar, upon learning of the meeting, became concerned of a potential coup and hastened to the gathering. Upon arriving, Abu Bakr addressed the assembled men with a warning that an attempt to elect a leader outside of Muhammad's own tribe, the Quraysh, would likely result in dissension, as only they can command the necessary respect among the community. He then took Umar and Abu Ubaidah by the hand and offered them to the Ansar as potential choices. [[Al-Hubab ibn al-Mundhir|Habab ibn Mundhir]], a veteran from the [[Battle of Badr]], countered with his own suggestion that the Quraysh and the Ansar choose a leader each from among themselves, who would then rule jointly. The group grew heated upon hearing this proposal and began to argue amongst themselves.<ref name=MadelungP30-31>{{harvtxt|Madelung|1997|pp=30–31}}</ref> [[William Muir]] gives the following observation of the situation:<ref>William Muir, ''The Caliphate - Its Rise, Decline, and Fall'' (1891), p. 2</ref>

{{blockquote|The moment was critical. The unity of the Faith was at stake. A divided power would fall to pieces, and all might be lost. The mantle of the Prophet must fall upon one Successor, and on one alone. The sovereignty of Islam demanded an undivided Caliphate, and Arabia would acknowledge no master but from amongst Quraysh.}}

Umar hastily took Abu Bakr's hand and swore his own allegiance to the latter, an example followed by the gathered men. The meeting broke up when a violent scuffle erupted between Umar and the chief of the Banu Sa'ida, [[Saʽd ibn ʽUbadah|Sa'd ibn Ubadah]]. This event suggests that the choice of Abu Bakr was not unanimous, with emotions running high as a result of the disagreement.<ref name="MandelungP32">{{harvtxt|Madelung|1997|page=32}}</ref>

Abu Bakr was near-universally accepted as head of the Muslim community (under the title of Caliph) as a result of Saqifah, though he did face contention because of the rushed nature of the event. Several companions, most prominent among them being Ali, initially refused to acknowledge his authority.{{sfn|Fitzpatrick|Walker|2014|p=3}} Among Shi'ites, it is also argued that Ali had [[Ghadir Khumm|previously been appointed]] as Muhammad's heir, with the election being seen as in contravention to the latter's wishes.<ref>{{cite book |first1=Bernhard |last1=Platzdasch |first2=Johan |last2=Saravanamuttu |title=Religious Diversity in Muslim-majority States in Southeast Asia - Areas of Toleration and Conflict |url=https://books.google.com/books?id=7ThpBgAAQBAJ&pg=PA364 |date=6 August 2014 |publisher=Institute of Southeast Asian Studies |isbn=978-981-4519-64-9 |page=364}}</ref> Abu Bakr later sent Umar to confront Ali, resulting in [[Attack on Fatima's house|an altercation]] which may have involved violence.{{sfn|Fitzpatrick|Walker|2014|p=186}} However, after six months the group made peace with Abu Bakr and Ali offered him his allegiance.{{sfn|Fitzpatrick|Walker|2014|p=4}}

=== Accession ===
After assuming the office of Caliph, Abu Bakr's first address was as follows:

{{blockquote|I have been given the authority over you, and I am not the best of you. If I do well, help me, and if I do wrong, set me right. Sincere regard for truth is loyalty and disregard for truth is treachery. The weak amongst you shall be strong with me until I have secured his rights, if God wills, and the strong amongst you shall be weak with me until I have wrested from him the rights of others, if God wills. Obey me so long as I obey God and His Messenger. But if I disobey God and His Messenger, you owe me no obedience. Arise for your prayer, God have mercy upon you. (Al-Bidaayah wan-Nihaayah 6:305, 306)}}

Abu Bakr's reign lasted for 27 months, during which he crushed the rebellion of the Arab tribes throughout the [[Arabian Peninsula]] in the successful [[Ridda Wars|Ridda wars]]. In the last months of his rule, he sent Khalid ibn al-Walid on conquests [[Muslim conquest of Persia|against the Sassanid Empire]] in [[Mesopotamia]] and [[Muslim conquest of Syria|against the Byzantine Empire]] in [[Syria (region)|Syria]]. This would set in motion a historical trajectory<ref name="Donner">{{Cite book |url=https://books.google.com/books?id=qBzRj7OajmEC&pg=PP1 |title=Muhammad and the Believers - At the Origins of Islam |first1=Fred M. |last1=Donner |first2=Professor of Near Eastern History in the Oriental Institute and Department of Near Eastern Languages and Civilizations Fred M. |last2=Donner |date=15 May 2010 |publisher=[[Harvard University Press]] |isbn=9780674050976 |via=Google Books}}</ref> (continued later on by [[Umar]] and [[Uthman]]) that in just a few short decades would lead to one of the [[List of largest empires|largest empires in history]]. He had little time to pay attention to the administration of state, though state affairs remained stable during his Caliphate. On the advice of Umar and Abu Ubaidah ibn al-Jarrah, he agreed to draw a salary from the state treasury and discontinue his cloth trade.

=== Ridda wars ===
{{Main|Ridda Wars}}

[[File:Caliph Abu Bakr's empire at its peak 634-mohammad adil rais.PNG|thumb|upright=1.2|Abu Bakr's caliphate at its territorial peak in August 634]]

Troubles emerged soon after Abu Bakr's succession, with several Arab tribes launching revolts, threatening the unity and stability of the new community and state. These insurgencies and the caliphate's responses to them are collectively referred to as the Ridda wars ("Wars of Apostasy").<ref name=DonnerP85>{{cite book |last=Donner |first=Fred M. |author-link=Fred Donner |title=The Early Islamic Conquests |publisher=[[Princeton University Press]] |date=1981 |page=85 |isbn=9781400847877 |url=https://books.google.com/books?id=l5__AwAAQBAJ&pg=PA85}}</ref>

The opposition movements came in two forms. One type challenged the political power of the nascent caliphate as well as the religious authority of Islam with the acclamation of rival ideologies, headed by political leaders who claimed the mantle of prophethood in the manner that Muhammad had done. These rebellions include:<ref name=DonnerP85/>
* that of the [[Banu Asad]] headed by [[Tulayha ibn Khuwaylid]];
* that of the [[Banu Hanifa]] headed by [[Musaylima]];
* those from among the [[Taghlib]] and the [[Banu Tamim]] headed by [[Sajah]];
* that of the [[Al-Ansi]] headed by [[Al-Aswad al-Ansi]].

These leaders are all denounced in Islamic histories as "false prophets".<ref name=DonnerP85/>

The second form of opposition movement was more strictly political in character. Some of the revolts of this type took the form of tax rebellions in [[Najd]] among tribes such as the [[Banu Fazara]] and Banu Tamim. Other dissenters, while initially allied to the Muslims, used Muhammad's death as an opportunity to attempt to restrict the growth of the new Islamic state. They include some of the [[Rabi'a ibn Nizar]] in [[Eastern Arabia]], the [[Azd]] in [[History of Oman|Oman]], as well as among the [[Kinda (tribe)|Kinda]] and [[Khawlan]] in [[South Arabia|Yemen]].<ref name=DonnerP85/>

Abu Bakr, likely understanding that maintaining firm control over the disparate tribes of Arabia was crucial to ensuring the survival of the state, suppressed the insurrections with military force. He dispatched [[Khalid ibn al-Walid]] and a body of troops to subdue the uprisings in Najd as well as that of Musaylimah, who posed the most serious threat. Concurrent to this, [[Shurahbil ibn Hasana]] and [[Al-Ala ibn al-Hadrami|Al-Ala'a ibn al-Hadrami]] were sent to Bahrayn, while [[Ikrima ibn Amr|Ikrima ibn Abi Jahl]], [[Hudhayfah al-Bariqi]] and [[Arfajah|Arfaja al-Bariqi]] were instructed to conquer Oman. Finally, [[Al-Muhajir ibn Abi Umayya]] and Khalid ibn Asid were sent to Yemen to aid the local governor in re-establishing control. Abu Bakr also made use of diplomatic means in addition to military measures. Like Muhammad before him, he used [[Marriage of state|marriage alliances]] and financial incentives to bind former enemies to the caliphate. For instance, a member of the Banu Hanifa who sided with the Muslims was rewarded with the granting of a land estate. Similarly, a Kindah rebel named [[Al-Ash'ath ibn Qays]], after repenting and re-joining Islam, was later given land in Medina as well as the hand of Abu Bakr's sister Umm Farwa in marriage.<ref>{{harvtxt|Donner|1981|pages=86–87}}</ref>

At their heart, the Ridda movements were challenges to the political and religious supremacy of the Islamic state. Through his success in suppressing the insurrections, Abu Bakr had in effect continued the political consolidation which had begun under Muhammad's leadership with relatively little interruption. By wars' end, he had established an Islamic hegemony over the entirety of the [[Arabian Peninsula]].<ref>{{harvtxt|Donner|1981|page=86}}</ref>

=== Expeditions into Mesopotamia, Persia and Syria ===
With Arabia having united under a single centralised state with a formidable military, the region could now be viewed as a potential threat to the neighbouring [[Sasanian Empire|Sasanian]] and [[Byzantine Empire|Byzantine empires]]. It may be that Abu Bakr, reasoning that it was inevitable that one of these powers would launch a pre-emptive strike against the youthful caliphate, decided that it was better to deliver the first blow himself. Regardless of the caliph's motivations, in 633, small forces were dispatched into Iraq and [[Palestine (region)|Palestine]], capturing several towns. Though the Sasanians and Byzantines were certain to retaliate, Abu Bakr had reason to be confident; the two empires were militarily exhausted after centuries of war against each other, making it likely that any forces sent to Arabia would be diminished and weakened.<ref name=NardoP32>{{cite book |last=Nardo |first=Don |author-link=Don Nardo |title=The Islamic Empire |publisher=Lucent Books |date=2011 |pages=30–32 |isbn=9781420506341 |url=https://archive.org/details/islamicempire0000nard |url-access=registration}}</ref>

An even more pressing advantage was the effectiveness and zeal of the Muslim fighters, the latter of which was partially based on their certainty of the righteousness of their cause. Additionally, the general belief among the Muslims was that the community must be defended at all costs. Though Abu Bakr had started these initial conflicts which eventually resulted in the Islamic [[Muslim conquest of Persia|conquests of Persia]] and [[Muslim conquest of Syria|the Levant]], he did not live to see those regions conquered by Islam, instead leaving the task to his successors.<ref name="NardoP32" />

=== Preservation of the Quran ===
{{Main|History of the Quran}}

Abu Bakr was instrumental in preserving the Quran in written form. It is said that after the hard-won victory over [[Musaylima]] in the [[Battle of al-Yamama]] in 632, [[Umar]] saw that some five hundred of the Muslims who had [[Hafiz (Quran)|memorised the Quran]] had been killed in wars. Fearing that it might become lost or corrupted, Umar requested that Abu Bakr authorise the compilation and preservation of the scriptures in written format. The caliph was initially hesitant, being quoted as saying, "how can we do that which the Messenger of Allah, may Allah bless and keep him, did not himself do?" He eventually relented, however, and appointed [[Zayd ibn Thabit]], who had previously served as one of the scribes of Muhammad, for the task of gathering the scattered verses. The fragments were recovered from every quarter, including from the ribs of palm branches, scraps of leather, stone tablets and "from the hearts of men". The collected work was transcribed onto sheets and verified through comparison with Quran memorisers.<ref>{{cite book |last1=Fernhout |first1=Rein |last2=Jansen |first2=Henry |last3=Jansen-Hofland |first3=Lucy |title=Canonical Texts. Bearers of Absolute Authority. Bible, Koran, Veda, Tipitaka: a Phenomenological Study |year=1994 |page=62 |publisher=[[Brill Publishers|Rodopi]] |isbn=9051837747 |url=https://books.google.com/books?id=BIIk_73ImdsC&pg=PA62}}</ref><ref>{{cite book |last=Herlihy |first=John |title=Islam for Our Time - Inside the Traditional World of Islamic Spirituality |date=2012 |page=76 |publisher=[[Xlibris|Xlibris Corporation]] |isbn=9781479709977 |url=https://books.google.com/books?id=lcb5AAAAQBAJ&pg=PA76}}</ref> The finished codex, termed the ''[[Mushaf|Mus'haf]]'', was presented to Abu Bakr, who prior to his death, bequeathed it to his successor Umar.<ref>{{cite book |last=Azmayesh |first=Seyed Mostafa |author-link=Seyed Mostafa Azmayesh |title=New Researches on the Quran - Why and how two versions of Islam entered the history of mankind |date=2015 |publisher=Mehraby Publishing House |page=75 |isbn=9780955811760 |url=https://books.google.com/books?id=ED1lCwAAQBAJ&pg=PA75}}</ref> Upon Umar's own death, the ''Mus'haf'' was left to his daughter [[Hafsa bint Umar|Hafsa]], who had been one of the wives of Muhammad. It was this volume, borrowed from Hafsa, which formed the basis of [[Uthman]]'s prototype, which became the definitive text of the Quran. All later editions are derived from this original.<ref>{{harvtxt|Herlihy|2012|page=76–77}}</ref>{{NoteTag|Many early sources, especially but not exclusively [[Shia|Shi'ite]], believe that there was also a version of the Quran which had been compiled by Ali, but which has since been lost.<ref>{{harvtxt|Herlihy|2012|page=77}}</ref>}}

== Death ==
[[File:Abu Bakr dying.jpg|thumb|A 19th-century miniature from a manuscript of ''Hamla-i Haydari'', depicting Abu Bakr dying in the presence of [[Ali]].]]

On 23 August 634, Abu Bakr fell sick and did not recover. He developed a high fever and was confined to bed. His illness was prolonged, and when his condition worsened, he felt that his end was near. Realising this, he sent for Ali and requested him to perform his [[ghusl]] since Ali had also done it for Muhammad.

Abu Bakr felt that he should nominate his successor so that the issue should not be a cause of dissension among the Muslims after his death. Though there was already controversy over Ali not having been appointed,<ref>''Sidiq-i-Akbar Hazrat Abu Bakr'' by Masudul Hasan, [[Ferozsons]], 1976 {{OCLC|3478821}}</ref> He appointed Umar for this role after discussing the matter with some companions. Some of them favoured the nomination and others disliked it due to the tough nature of Umar.

Abu Bakr thus dictated his last testament to [[Uthman|Uthman ibn Affan]] as follows:

{{Blockquote|In the name of Most Merciful God. This is the last will and testament of Abu Bakr bin Abu Quhafa, when he is in the last hour of the world, and the first of the next; an hour in which the infidel must believe, the wicked be convinced of their evil ways, I nominate Umar ibn al Khattab as my successor. Therefore, hear to him and obey him. If he acts right, confirm his actions. My intentions are good, but I cannot see the future results. However, those who do ill shall render themselves liable to severe account hereafter. Fare you well. May you be ever attended by the Divine favor of blessing.<ref>{{Cite web|url=http://www.alim.org/library/biography/khalifa/content/KAB/18/2|title=Islamic history of Khalifa Abu Bakr – Death of Abu Bakr &#124; Al Quran Translations &#124; Alim|website=www.alim.org|access-date=16 June 2010|archive-date=31 October 2020|archive-url=https://web.archive.org/web/20201031131259/http://www.alim.org/library/biography/khalifa/content/KAB/18/2|url-status=dead}}</ref>}}

[[Umar]] led the [[Funeral prayer (Islam)|funeral prayer]] for him and he was buried beside the grave of Muhammad.<ref>{{Cite web |url=https://books.google.com/books?id=qkUkAQAAIAAJ&q=Umar+led+the+funeral+prayer+for+Abubakar,+and+he+was+buried+with+the+Muhammad |title=Islamic Review |date=15 April 1967 |publisher=Shah Jehan Mosque |via=Google Books}}</ref>

== Appearance ==
The historian [[Al-Tabari]], in regards to Abu Bakr's appearance, records the following interaction between Aisha and her paternal nephew, Abd Allah ibn Abd al-Rahman ibn Abi Bakr:<ref name=TabariBlankinshipP138-39>{{cite book |last1=Al-Tabari |first1=Muhammad ibn Jarir |last2=Blankinship |first2=Khalid Yahya |author-link1=Al-Tabari |author-link2=Khalid Yahya Blankinship |title=The History of al-Tabari, Volume XI - The Challenge to the Empires |date=1993 |pages=138–139 |url=https://archive.org/stream/TabariEnglish/Tabari_Volume_11#page/n175/mode/2up}}</ref>

<blockquote> When she was in her [[howdah]] and saw a man from among the Arabs passing by, she said, "I have not seen a man more like Abu Bakr than this one." We said to her, "Describe Abu Bakr." She said, "A slight, white man, thin-bearded and bowed. His waist wrapper would not hold but would fall down around his loins. He had a lean face, sunken eyes, a bulging forehead, and trembling knuckles".</blockquote>

Referencing another source, Al-Tabari further describes him as being "white mixed with yellowness, of good build, slight, 
bowed, thin, tall like a male palm tree, hook-nosed, lean-faced, sunken-eyed, thin-shanked, and strong-thighed. He used to dye himself with [[henna]] and black dye".<ref name=TabariBlankinshipP138-39/>

== Assessment and legacy ==
Although Abu Bakr's caliphate lasted only two years, two months, and fifteen days, it encompassed successful campaigns against the [[Sasanian Empire|Sasanian]] and [[Byzantine Empire|Byzantine empires]], the two most powerful empires of the era. He is known by the titles as ''[[Siddiq|Al-Siddiq]], [[Atiq]]'' and ''Companion of the Cave''.<ref>{{cite encyclopedia |title=YÂR-ı GĀR (Companion of the cave) |encyclopedia=[[İslâm Ansiklopedisi|TDV Encyclopedia of Islam]] |url=https://islamansiklopedisi.org.tr/yar-i-gar |date=2013 |lang=tr |last1=İsmet Uzun |first1=Mustafa}}</ref>  As the first caliph in [[History of Islam|Islamic history]], Abu Bakr was also the first to nominate a successor. He returned his entire caliphal allowance to the state [[treasury]] upon his death, a unique act among caliphs.<ref name="archive.org" /> Notably, he purchased the land for [[Prophet's Mosque|Al-Masjid al-Nabawi]].<ref>Iqbal, M. A. (2022, October). [https://www.dawateislami.net/magazine/en/pages-of-the-history/masjid-e-nabwi-construction The construction of Masjid e Nabvi.] ''Faizan-e-Madinah''. Dawat-e-Islami.</ref>

=== Sunni view ===
Sunni Muslim tradition considers Abu Bakr the best man after the prophets. He is also regarded as one of the Ten Promised Paradise (''[[The ten to whom Paradise was promised|al-'Ashara al-Mubashshara]]'') whom Muhammad testified were destined for Paradise. Abu Bakr is recognised as the "Successor of Allah's Messenger" (''Khalifa Rasulullah''), the first of the [[Rashidun|Rightly Guided Caliphs]] and the rightful successor to Muhammad. He was always the closest friend and confidant of Muhammad, accompanying him during every major event. Muhammad consistently honoured Abu Bakr's wisdom. He is regarded among the greatest of Muhammad's followers; as Umar ibn al-Khattab stated, "If the faith of Abu Bakr were weighed against the faith of the people of the earth, the faith of Abu Bakr would outweigh theirs."<ref>Narrated by al-Bayhaqi in "al-Jamia" lashu'ab al-Eemaan' (1:18) and its narrators are trustworthy.</ref>

=== Shia view ===

[[Shia Islam|Shia Muslims]] believe that Ali ibn Abi Talib was supposed to assume [[Caliphate|leadership]] and that he had been publicly and unambiguously appointed by Muhammad as his successor at [[Ghadir Khumm]]. It is also believed that Abu Bakr and Umar conspired to take over power in the Muslim nation after Muhammad's death in a coup d'état against Ali.

Most [[Twelver Shi'ism|Twelvers]] (as the main branch of Shia Islam, with 85% of all Shias)<ref>{{Cite web |title=Shia Islam's Holiest Sites |url=https://www.worldatlas.com/articles/shia-islam-s-holiest-sites.html |website=worldatlas.com |date=25 April 2017}}</ref><ref>{{cite web |url=https://www.al-islam.org/shiite-encyclopedia-ahlul-bayt-dilp-team/usurping-land-fadak |title=Usurping the Land of Fadak |website=al-islam.org |date=12 November 2013}}</ref><ref>{{cite web |url=https://www.al-islam.org/the-message-ayatullah-jafar-subhani/chapter-44-story-fadak |title=Chapter 44 - The Story of Fadak |website=al-islam.org |date=27 December 2012}}</ref><ref>{{cite web |url=https://www.twelvershia.net/2014/05/08/fadak-prophetic-inheritance-qa/ |title=Fadak and Inheritance Q&A |date=8 May 2014 |website=twelvershia.net}}</ref> have a negative view of Abu Bakr because, after Muhammad's death, Abu Bakr refused to grant Muhammad's daughter, [[Fatima]], the lands of the village of [[Fadak]] which she claimed her father gave to her as a gift before his death. He refused to accept the testimony of her witnesses, so she claimed the land would still belong to her as inheritance from her deceased father. However, Abu Bakr replied by saying that Muhammad told him that the prophets of God do not leave as inheritance any worldly possessions and on this basis he refused to give her the lands of Fadak.<ref>[http://www.al-islam.org/fatima-the-gracious-abu-muhammad-ordoni/abu-bakr-versus-fatima-az-zahra-sa al-islam.org], ''Fatima the Gracious'', by Abu – Muhammad Ordoni, 1987, Section entitled ''Abu Bakr Versus Fatima az-Zahra (sa)''.<br />See also ''Sahih Al Bukhari'' Volume 5, Book 57, Number 60, which says: "Fatima sent somebody to Abu Bakr asking him to give her her inheritance from the Prophet from what Allah had given to His Apostle through Fai (i.e. booty gained without fighting). She asked for the Sadaqa (i.e. wealth assigned for charitable purposes) of the Prophet at Medina, and Fadak, and what remained of the Khumus (i.e., one-fifth) of the Khaibar booty". Abu Bakr said, "Allah's Apostle said, "We (Prophets), our property is not inherited, and whatever we leave is Sadaqa, but Muhammad's Family can eat from this property, i.e. Allah's property, but they have no right to take more than the food they need". By Allah! I will not bring any change in dealing with the Sadaqa of the Prophet (and will keep them) as they used to be observed in his (i.e. the Prophet's) life-time, and I will dispose with it as Allah's Apostle used to do". Then Ali said, "I testify that None has the right to be worshipped but Allah, and that Muhammad is His Apostle", and added, "O Abu Bakr! We acknowledge your superiority". Then he (i.e. Ali) mentioned their own relationship to Allah's Apostle and their right. Abu Bakr then spoke saying, "By Allah in Whose Hands my life is. I love to do good to the relatives of Allah's Apostle rather than to my own relatives". Abu Bakr added: Look at Muhammad through his family".<br />See also ''Sahih Al Bukhari'' Volume 8, Book 80, Number 722, which says: Aisha said, "When Allah's Apostle died, his wives intended to send Uthman to Abu Bakr asking him for their share of the inheritance". Then Aisha said to them, "Didn't Allah's Apostle say, Our (Apostles') property is not to be inherited, and whatever we leave is to be spent in charity?"</ref> However, as Sayed Ali Asgher Razwy notes in his book ''A Restatement of the History of Islam & Muslims'', Muhammad inherited a maid servant, five camels, and ten sheep. Shia Muslims believe that prophets can receive inheritance, and can pass on inheritance to others as well.<ref>{{cite book |last1=Razwy |first1=Ali Asgher |title=A Restatement of the History of Islam & Muslims |pages=34–35}}</ref> In addition, Shias claim that Muhammad had given Fadak to Fatimah during his lifetime,<ref>{{cite book |last1=Jalālī |first1=Ḥusaynī |title=Fadak wa l-ʿawālī |page=141}}</ref> and Fadak was therefore a gift to Fatimah, not inheritance. This view has also been supported by the Abbasid ruler [[al-Ma'mun]].<ref>{{cite book |last1=Shahīdī |title=Zindigānī-yi Fātima-yi Zahrā |page=117}}</ref>

Twelvers also accuse Abu Bakr of participating in the [[attack on Fatima's house]].<ref>Ibn Qutayba al Dinawari. Al Imama Wa'l Siyasa.</ref> The Twelver Shia believe that Abu Bakr sent [[Khalid ibn al-Walid]] to crush those who were in favour of Ali's caliphate (''see [[Ridda Wars]]''). The Twelver Shia strongly contest the idea that Abu Bakr or Umar were instrumental in the collection or preservation of the ''Quran'', claiming that they should have accepted the copy of the book in the possession of Ali.<ref>[http://al-islam.org/encyclopedia/chapter8/4.html al-islam.org], ''The Quran Compiled by Imam Ali (AS)''</ref>

However, Sunnis argue that Ali and Abu Bakr were not enemies and that Ali named his sons Abi Bakr in honour of Abu Bakr.<ref>{{Cite web |title=The names of Imam Ali (as)'s sons |url=http://names-of-imam-ali-sons.html/ |access-date=13 August 2021 |language=en}}{{Dead link |date=November 2023 |bot=InternetArchiveBot |fix-attempted=yes}}</ref> After the death of Abu Bakr, Ali raised Abu Bakr's son [[Muhammad ibn Abi Bakr]]. The Twelver Shia view Muhammad as one of the greatest companions of Ali.<ref name="ReferenceB">Nahj al-Balagha Sermon 71, Letter 27, Letter 34, Letter 35</ref> When he was killed by the [[Umayyad dynasty|Umayyads]],<ref name="ReferenceB" /> Aisha, the third wife of Muhammad (the prophet), raised and taught her nephew [[Qasim ibn Muhammad ibn Abi Bakr]]. Qasim's mother was from Ali's family and his daughter [[Umm Farwa|Farwah bint al-Qasim]] was married to [[Muhammad al-Baqir]] and was the mother of [[Ja'far al-Sadiq]]. Therefore, Qasim was the grandson of Abu Bakr and the grandfather of Ja'far al-Sadiq.

[[Zaydism|Zaydi Shias]], the largest group amongst the Shia before the [[Safavid dynasty]] and currently the second-largest group (although its population is only about 5% of all Shia Muslims),<ref>{{Cite web |url=https://2009-2017.state.gov/documents/organization/208632.pdf |archive-url=https://ghostarchive.org/archive/20221009/https://2009-2017.state.gov/documents/organization/208632.pdf |archive-date=9 October 2022 |url-status=live |title=state.gov}}</ref><ref>Stephen W. Day (2012), Regionalism and Rebellion in Yemen - A Troubled National Union, [[Cambridge University Press]], p. 31 {{ISBN|9781107022157}} Jump up</ref><ref>"Mapping the Global Muslim Population - A Report on the Size and Distribution of the World's Muslim Population", Pew Research Center, 7 October 2009, retrieved 25 August 2010.</ref> believe that on the last hour of [[Zayd ibn Ali]] (the uncle of Ja'far al-Sadiq), he was betrayed by the people in [[Kufa]] who said to him: "May God have mercy on you! What do you have to say on the matter of Abu Bakr and Umar ibn al-Khattab?" Zayd ibn Ali said, "I have not heard anyone in my family renouncing them both nor saying anything but good about them [...] when they were entrusted with government they behaved justly with the people and acted according to the Quran and the Sunnah".<ref name="Najeebabadi">Akbar Shah Najeebabadi, The history of Islam, B0006RTNB4.</ref><ref>The waning of the Umayyad caliphate by Tabarī, Carole Hillenbrand, 1989, p. 37–38</ref><ref>The Encyclopedia of Religion, Vol. 16, Mircea Eliade, Charles J. Adams, Macmillan, 1987, p. 243, "They were called Rafida by the followers of Zayd"</ref>

In a similar view, the [[Ismailism|Ismaili Shias]] under the leadership of the [[Aga Khan]]s have also come to accept the caliphates of the first three caliphs, including that of Abu Bakr:

{{Blockquote|"In the present Imamat, the final reconciliation between the Shia and Sunni doctrines has been publicly proclaimed by myself on exactly the same lines as [[Ali|Hazrat Aly]] did at the death of the Prophet and during the first thirty years after that. '''The political and worldly Khalifat was accepted by Hazrat Aly in favour of the three first Khalifs voluntarily and with goodwill for the protection of the interests of the Muslims throughout the world. We Ismailis now in the same spirit accept the Khalifat of the first Khalifs''' and such other Khalifs as during the last thirteen centuries helped the cause of Islam, politically, socially and from a worldly point of view. On the other hand, the Spiritual Imamat remained with Hazrat Aly and remains with his direct descendants always alive till the day of Judgement" |author=''Aga Khan III - Selected Speeches and Writings of Sir Sultan Muhammad Shah'', p. 1417<ref>{{Cite book |last=Aga Khan III |title=Selected Speeches and Writings of Sir Sultan Muhammad Shah |publisher=Kegan Paul |date=1998 |isbn=0710304277 |page=1417}}</ref>}}

== Notes ==
{{Notelist}}
{{Notefoot}}

== References ==
{{Reflist}}

== Bibliography ==
* {{cite book |last1=Fitzpatrick |first1=Coeli |last2=Walker |first2=Adam Hani |title=Muhammad in History, Thought, and Culture - An Encyclopedia of the Prophet of God |date=2014 |publisher=[[Bloomsbury Publishing]] |isbn=9781610691789}}
* Walker, Adam, Abu Bakr al-Siddiq, in ''Muhammad in History, Thought, and Culture - An Encyclopedia of the Prophet of God'' (2 vols.), edited by C. Fitzpatrick and A. Walker, Santa Barbara, [[ABC-Clio]], 2014.
* {{citation |first=Barnaby |last=Rogerson |author-link=Barnaby Rogerson |url=https://books.google.com/books?id=ExbdVf5fFmUC |title=The Heirs of the Prophet Muhammad - And the Roots of the Sunni-Shia Schism |date=4 November 2010 |publisher=Little, Brown Book Group |isbn=978-0-74-812470-1}}
* {{citation |first=Barnaby |last=Rogerson |url=https://books.google.com/books?id=qzyBPwAACAAJ |title=The Heirs of Muhammad - Islam's First Century and the Origins of the Sunni-Shia Split |date=2008 |publisher=Overlook |isbn=978-1-59-020022-3}}
* {{citation |first=Wilferd |last=Madelung |url=https://books.google.com/books?id=2QKBUwBUWWkC |title=The Succession to Muhammad - A Study of the Early Caliphate |date=15 October 1998 |publisher=[[Cambridge University Press]] |isbn=978-0-52-164696-3}}
* {{Citation |last=Huthayfa |first=Abu |title=Abu Bakr - The First Caliph |url=https://books.google.com/books?id=xZ4bnQEACAAJ&q=abu+bakr |date=2013 |publisher=Al Qasim |isbn=9780958172035}}
* {{cite encyclopedia |date=2015 |title=Amīr al-ḥajj |encyclopedia=The Encyclopedia of Islam, THREE |publisher=[[Brill Publishers|BRILL Online]] |url=http://referenceworks.brillonline.com/entries/encyclopaedia-of-islam-3/ami-r-al-h-ajj-COM_24219 |last=Hathaway |first=Jane |editor1=Kate Fleet |editor2=Gudrun Krämer |editor3=Denis Matringe |editor4=John Nawas |editor5=Everett Rowson}}
* [https://www.britannica.com/biography/Abu-Bakr Abū Bakr Muslim caliph], in ''Encyclopædia Britannica Online'', by The Editors of Encyclopædia Britannica, Yamini Chauhan, Aakanksha Gaur, Gloria Lotha, Noah Tesch and Amy Tikkanen
* {{cite encyclopedia |date=2022 |title=Al-Saḳīfa |encyclopedia=Encyclopaedia of Islam |publisher=[[Brill Publishers|Brill Reference Online]] |url=https://referenceworks.brillonline.com/entries/encyclopaedia-of-islam-2/al-sakifa-COM_0980?s.num=1&s.f.s2_parent=s.f.cluster.Encyclopaedia+of+Islam&s.q=sakifa |editor-last=Bearman |editor-first=P. |edition=Second |author-last=Lecomte |author-first=G.}}

== External links ==
{{Commons}}
{{wikisource|works=or}}
{{Wikiquote}}
{{EB1911 poster|Abu-Bekr}}

{{S-start}}
{{S-hou|[[Banu Taim]]||27 October 573||22 August 634|[[Quraysh (tribe)|Quraysh]]}}
{{S-rel|su}}
{{S-bef|before=[[Muhammad]]|as=[[Khatam an-Nabiyyin|Final prophet]]|rows=4}}
{{S-ttl|title=[[Caliphate|Caliph of Islam]]<br />[[Rashidun Caliph]]|years=8 June 632{{snd}}22 August 634}}
{{S-aft|after=[[Umar ibn Al-Khattab]]}}
{{s-end}}

{{Rashidun Caliphs}}
{{Ten companions of Muhammad}}

{{Authority control}}

[[Category:Abu Bakr| ]]
[[Category:573 births]]
[[Category:634 deaths]]
[[Category:Arab Muslims]]
[[Category:People from Mecca]]
[[Category:Rashidun caliphs]]
[[Category:Family of Abu Bakr| ]]
[[Category:7th-century caliphs]]
[[Category:Sahabah who participated in the battle of Uhud]]
[[Category:Sahabah who participated in the battle of Badr]]
[[Category:People of the Muslim conquest of the Levant]]
[[Category:Arab slave owners]]
[[Category:Sahabah hadith narrators]]
[[Category:Burials at Al-Masjid an-Nabawi]]
[[Category:7th-century monarchs in Asia]]
[[Category:Banu Taym]]`,
  `<ref name="https://pt.scribd.com/document/742344591/smartproxy-cities"/>`,
  `[[Mushroom poisoning|deadly mushrooms]]`,
  `; A'''B''' : C`,
  `; A''B'' : C`,
  `; A'''''B''''' : C`,
  `; outer '''bold : text''' end`,
  `＿＿目次＿＿`,
  `text ＿＿目次＿＿ more text`,
  `  ＿＿目次＿＿  `,
  '== Heading == ',
  `== Heading ==\xC2\xA0`,
  `== Heading ==\xE3\x80\x80`,
  `== Normal Heading ==`,
  `=== Nested ===  `,
  `== Heading\xC2\xA0text ==`,
  `[[/Sub]]`,
  `[[/Sub|display text]]`,
  `[[../Sibling]]`,
  `[[#Section]]`,
  `{{Template}} and [[/Subpage]]`,
  `#REDIRECT [[WP:Foo]]`,
  `#REDIRECT [[Project:Shortcut]]`,
  `#REDIRECT [[en:English article]]`,
  `#REDIRECT [[Target page|display]]`,
  `[[File:Foo.jpg|link=<bad>|right|Caption]]`,
  `[[File:Foo.jpg|notapixelvalue|right|Caption]]`,
  `[[File:Foo.jpg|200px|right|Caption]]`,
  `[[File:Foo.jpg|200x150px]]`,
  `[[File:Diagram.svg|lang=zh]]`,
  `[[File:Photo.jpg|lang=zh]]`,
  `[[File:Book.djvu|page=3]]`,
  `[[File:Photo.jpg|page=3]]`,
  `[[File:Foo.jpg|link=Main Page]]`,
  `[[File:Foo.jpg|link=]]`,
  `[[File:Foo.jpg|border|200px|right|Caption]]`,
  `-{zh:简体;zh-hant:繁體}-`,
  `Before -{A|B}- after`,
  `{{T|-{A|B}-}}`,
  `<ref>-{A|B}-</ref>`,
  `<gallery>\nFile:Foo.jpg|Caption <!-- hidden --> text\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|''italic'' and '''bold''' caption\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|See [http://example.com link]\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|See RFC 2119\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|{{Template|arg}} caption\n</gallery>`,
  `<imagemap>\nFile:Foo.jpg|thumb|''italic'' caption\npoly 0 0 10 10 [[Link]]\n</imagemap>`,
  `<imagemap>\nFile:Foo.jpg|See RFC 2119\npoly 0 0 10 10 [[Link]]\n</imagemap>`,
  `<poem>\n* First item\n* Second item\n</poem>`,
  `<poem>* First item\n* Second item</poem>`,
  `<poem>\n# Numbered first\n# Numbered second\n</poem>`,
  `<poem>\n; Term\n: Definition\n</poem>`,
  `[[{{okina}}Iolani Palace]]`,
  `[[/|\]] (Back slash)`,
  `<ref name="The Pulitzer Prizes {{pipe}} Poetry">{{Cite web |title=The Pulitzer Prizes {{pipe}} Poetry |url=http://www.pulitzer.org/bycat/Poetry |access-date=October 31, 2010 |publisher=Pulitzer.org}}</ref>`,
  `{{#tag:something|{{#expr:1+1}}|{{#time:Y}}}}`,
  `<gallery caption="Prints from [[Bernard de Montfaucon]]'s ''L'antiquité expliquée et représentée en figures'' (Band 2,2) page 358 ff.">
  File:Montfaucon Abraxas Plaque 149.xcf|Plaque 149
  </gallery>`,
  `{{#tag:timeline|
  ImageSize  = width:1500 height:auto barincrement:18
  PlotArea   = top:10 bottom:20 right:130 left:10
  AlignBars  = late
  DateFormat = x.y
  Period     = from:1816.90 till:{{#expr:{{#time:Y}}+{{#time:m}}/6}}
  TimeAxis   = orientation:horizontal
  ScaleMajor = unit:year increment:10 start:1820
  ScaleMinor = unit:year increment:1 start:1817

  Define $now = {{#expr:{{#time:Y}}+{{#time:m}}/12}}

  Colors =
    id:5year   value:rgb(0.8, 0.8, 0.8)

  BarData =
    barset:GovernorLine
    barset:Governors
    #barset:blankline

  PlotData=
  width:1 align:right fontsize:S shift:(-3,-4) anchor:from fontsize:8 color:black

  barset:GovernorLine
  from:1832 till:end text:Governors

  width:6 align:left fontsize:S shift:(5,-4) anchor:till fontsize:10

  barset:Governors
  from:1819.86 till:1820.52 color:demrep text:"William W. Bibb"
  from:1820.52 till:1821.86 color:demrep text:"Thomas Bibb"
  from:1821.86 till:1825.9 color:demrep text:"Israel Pickens"
  from:1825.9 till:1829.89 color:jackson text:"John Murphy"

  LineData=
  from:1817.73 till:1819.86 atpos:989 color:noparty width:6 # WWB noparty
  from:1831.9 till:1833 atpos:883 color:jackson width:6 # JG jackson

  layer:back
  # This section creates the vertical lines.
  at:1820.00 width:0.1 color:0year
  at:1825.00 width:0.1 color:5year
  at:1830.00 width:0.1 color:0year
  at:1835.00 width:0.1 color:5year
  at:1840.00 width:0.1 color:0year
  at:1845.00 width:0.1 color:5year
  at:1850.00 width:0.1 color:0year
  }}`,
  `<Gallery>
  File:The Odeon of Herodes Atticus on September 13, 2020.jpg|The [[Odeon of Herodes Atticus]] built in AD 161 by [[Herodes Atticus]]
  </Gallery>`,
  `archive-url=https://web.archive.org/web/20160325000841/http://danmarkshistorien.dk/leksikon-og-kilder/vis/materiale/aarhus-domkirke/?chash=a83dfe233ede9644167a9b5ea9aa35a4&tx_historyview_pi1&#91;lang&#93;=1`,
  ` [[File:Maaloula square alef.svg|20px|]]`,
  `[[File:Uranocene-3D-balls.png|thumb|upright=0.55|Predicted structure of amerocene [(η<sup>8</sup>-C<sub>8</sub>H<sub>8</sub>)<sub>2</sub><nowiki>Am]</nowiki>]]`,
  `|- 1 CAR
  ! 1
  ! rowspan="4" | Position
  | style="background:#dfffdf;" | 4
  | style="background:#dfffdf;" | 4`,
  `{|
  |- 1 CAR
  ! 1
  ! rowspan="4" | Position
  | style="background:#dfffdf;" | 4
  | style="background:#dfffdf;" | 4
  |}`,
  `{|
  |- {{T}} CAR
  |}`,
  `{|
  |- -{zh-hans:a;zh-hant:b;}- CAR
  |}`,
  `{|
  |- <!--c--> 1 CAR
  |}`,
  `== References ==
  {{Reflist}}
  {{refbegin}}
  * {{Brooklands: On Audi & Auto Union 1980|editor-mask=6}}
  {{refend}}
  `,
  `[[ζ Arietis]]`,
  `<imagemap>
  File:Actinopterygii.jpg||300px
  rect 0 0 333 232 [[Electrophorus electricus|Electric eel]]
  </imagemap>`,
  `<imagemap>
  File:Foo.jpg|thumb||alt=A
  poly 1 1 2 2 [[L|t]]
  </imagemap>`,
  `<imagemap>
  File:Foo.jpg|||alt=A
  poly 1 1 2 2 [[L|t]]
  </imagemap>`,
  `<ref>Andy Pease, [http://windliterature.org/2014/07/01/america-the-beautiful-by-katharine-lee-bates-and-samuel-augustus-ward-arr-carmen-dragon/ {{"'}}America the Beautiful' by Katharine Lee Bates and Samuel Augustus Ward, arr. Carmen Dragon"] ({{webarchive|url=https://web.archive.org/web/20180222162222/http://windliterature.org/2014/07/01/america-the-beautiful-by-katharine-lee-bates-and-samuel-augustus-ward-arr-carmen-dragon/ |date=February 22, 2018}}), Wind Band Literature, July 1, 2014; accessed 2019-08-17.</ref>`,
  `{| class="wikitable sortable"
  ! colspan="3" |[[File:Diplomatic_relations_of_Angola.svg|frameless|425x425px]]
  |-
  !#
  !Country
  !Date
  |-
  |174
  |{{Flag|Bahamas}}
  |{{dts|26 September 2025}}<ref>{{Cite web |date=26 September 2025 |title=We established diplomatic relations with Angola by signing a memorandum to that effect in New York at the United Nations. |url=https://www.facebook.com/fredmitchellmbm/posts/pfbid02KWQbzWBsS1WiabSQRmZJMMBf9Mv6584W6jaTTufWp341bGqqLNKoFie7wdvqc3JYl |access-date=26 September 2025 |website=Fred Mitchell - Minute By Minute on Facebook}}</ref>
  |}`,
  `<gallery mode="packed" caption="Road transport in Angola.">
  Midd Town Luanda.jpg|Automobiles in [[Luanda]].
  The Nowhere road.jpg|New highway (2019).
  </gallery>`,
  `<references>
  <ref name="Schopenhauer-2018">{{cite book |author-last=Schopenhauer |author-first=Arthur |date=2018 |title=The World as Will and Representation |orig-date=1844 |volume=2 |place=Cambridge |publisher=Cambridge University Press |doi=10.1017/9780511843112 |isbn=978-0-521-87034-4 |editor-last=Welchman |editor-first=Alistair |editor2-last=Janaway |editor2-first=Christopher |editor3-last=Norman |editor3-first=Judith |author-link1=Arthur Schopenhauer |title-link=The World as Will and Representation}}</ref>
  </references>`,
  `
  | style="text-align:center;"| [[File:Emblem of Qatar-2022.svg|20px|Link=Emblem of Qatar|alt=Emblem]]`,
  `|<poem style="margin-left:1em;">1911 version<ref>{{cite book |url=https://archive.org/details/americabeautiful00baterich |last=Bates |first=Katharine Lee |date=1911 |title=America the Beautiful and Other Poems |location=New York |publisher=Thomas Y. Crowell Company |pages=3–4 |via=archive.org}}</ref>sea!</poem>|}`,
  `{{Hatnote group|
  {{Redirect|Materna||Materna (disambiguation)|and|America the Beautiful (disambiguation)}}
  {{}}
  }}
  `,
  `{| class="wikitable" style="text-align:center"
  ! pentane || 2-methylbutane || 2,2-dimethylpropane
  |}`,
  `{{Cite web <!-- Citation bot changes to journal -->|url=https://www.cdc.gov/niosh/docs/2012-108/ |title=NIOSH Pesticide Poisoning Monitoring Program Protects Farmworkers |publisher=[[Centers for Disease Control and Prevention]] |access-date=15 April 2013 |url-status=live |archive-url=https://web.archive.org/web/20130402004253/http://www.cdc.gov/niosh/docs/2012%2D108/ |archive-date=2 April 2013|doi=10.26616/NIOSHPUB2012108 |year=2011 |doi-access=free}}`,
  `[[File:Andorra - panoramio (2).jpg|thumb|Streets of the city centre of Andorra la Vella in 1986. From 1986 until 1989 Andorra normalised the economic treaties with the [[European Economic Community|EEC]].<ref>{{cite news |date=18 December 1989 |title=La CE concluye un acuerdo de unión aduanera con Andorra |trans-title=The EC concludes a customs union agreement with Andorra |url=https://elpais.com/diario/1989/12/18/economia/629938809_850215.html |newspaper=El País |language=es}}</ref><ref>{{cite news |date=27 September 1986 |title=François Mitterrand alienta las reformas en Andorras |trans-title=François Mitterrand encourages reforms in Andorra |url=https://elpais.com/diario/1986/09/27/internacional/528156020_850215.html |newspaper=El País |language=es}}</ref>|alt=]]`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}| {{chset-cell1 | 124 U+007C: VERTICAL LINE | [[Vertical bar|{{pipe}}]] | style=background:#ffb2b2}}|}`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}
  | 
  }`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}|}`,
  `{{chset-cell1 | 124 U+007C: VERTICAL LINE | [[Vertical bar|{{pipe}}]] | style=background:#ffb2b2}}`,
  `{{chset-cell1 | 61 U+003D: EQUALS SIGN | [[=]] }}`,
  `| {{chset-cell1 | 123 U+007B: LEFT CURLY BRACKET | [[Left curly bracket|{]] | style=background:#ffffb2}}`,
  'This is a paragraph of plain text.',
  '#REDIRECT [[Target]] <!-- a comment -->',
  '{{Template|[[Page|label]]}}',
  '== {{PAGENAME}} ==\nContent.',
  '{|\n| [[Page|link]] || plain\n|}',
  '{|\n| alias = ISO-IR-006,<ref>reftext</ref> ANSI_X3.4-1968\n|}',
  '{|\n! Modern [[State of matter|state<br />of matter]]\n|}',
  '{{Outer|{{Inner|arg}}}}',
  "'''bold''' and ''[[Page|italic link]]''",
  '[[Page]] and [http://example.com external].',
  "See RFC 2119, [[Specification]] and http://example.com.",
  '* {{Template}}\n* plain item\n* [[Page]]',
  "== Before ==\n\n----\n\n== After ==",
  '== Relationship to surface [[bulk density]] ==',
  'Before __NOTOC__ after.',
  '{{Template|<!-- comment -->value}}',
  '<nowiki>[[not a link]] {{not a template}}</nowiki>',
  'Claim.<ref>{{Citation|title=Foo|year=2020}}</ref> More text.',
  "A<ref>''x'' efficacy is > <sub>2</sub>; ''y'' is > <sub>2</sub>.</ref>B",
  'A<ref>alpha<!--c--></ref>B',
  'A<ref>{{Citation|access-date= 29 June 2011<!--Added by DASHBot-->}}</ref>B',
  "{{T|v='''a\n''b'''}}",
  '<score raw=1 sound=1>\\relative c\'\' { x }</score>',
  '<ref>\n* a\n* b\n</ref>',
  "{{Blockquote|<poem><ref>x ''L'Etoile'' (as in ''H'')</ref></poem>}}",
  '{{Citation|title=Effect of land albedo, CO<sub>2</sub>, orography}}',
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970), historian [[Daniel Guérin]] describes [[libertarian socialism]].|group=nb}}",
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970),{{Sfn|Guérin|1970|p=12}} anarchist historian [[Daniel Guérin]] described it as a synonym for [[libertarian socialism]], and wrote that anarchism \"is really a synonym for socialism.\"{{Sfn|Arvidsson|2017}} In his many works on anarchism, historian [[Noam Chomsky]] describes anarchism, alongside [[libertarian Marxism]], as the [[libertarian]] wing of [[socialism]].{{Sfn|Otero|1994|p=617}}|group=nb}}",
  "'''Title''' (born [[1970]]) is a [[person]].\n\n== Career ==\n* [[Job A]]\n* [[Job B]]\n\n== References ==\n<references/>",
  '[[ẚ]]',
  "{{Wikisource-inline|list=\n** \"[[s:A Dictionary of the English Language/A|A]]\" in ''[[s:A Dictionary of the English Language|A Dictionary of the English Language]]'' by [[Samuel Johnson]]\n}}",
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe, Attic [[red-figure]] [[kantharos]] from [[Volci]] ([[Cabinet des Médailles]], Bibliothèque nationale, Paris)\n</gallery>',
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe\nFile:Akhilleus embassy Staatliche Antikensammlungen 8770.jpg|The embassy to Achilles, Attic red-figure [[hydria]]\n</gallery>',
  '(${{formatnum:{{Inflation|US|800|1861|r=-2}}}} in current dollars)',
  '<imagemap>\nFile:Emancipation proclamation.jpg|thumb|upright=1.25|\'\'[[First Reading of the Emancipation Proclamation of President Lincoln]]\'\'|alt=A dark-haired, bearded, middle-aged man holding documents is seated among seven other men.\npoly 269 892 254 775 193 738 [[Edwin M. Stanton|Edwin Stanton]]\n</imagemap>',
  "<ref>https://example.org/a</ref>",
  "<ref>RFC 2119</ref>",
  "<ref>----</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}-</ref>",
  "== https://example.org/a ==",
  "== RFC 2119 ==",
  "== -{zh-hans:简;zh-hant:繁;}- ==",
  "{{T|v=RFC 2119}}",
  "{{T|v=__NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}-}}",
  "{|\n| https://example.org/a\n|}",
  "{|\n| RFC 2119\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}-\n|}",
  "A -{zh-hans:简;zh-hant:繁;}- B",
  "<ref>{{C|title=RFC 2119}}</ref>",
  "<ref>{{C|title=__NOTOC__}}</ref>",
  "<ref>{{C|title=-{zh-hans:简;zh-hant:繁;}-}}</ref>",
  "<ref>plain ; https://example.org/a</ref>",
  "<ref>plain ; RFC 2119</ref>",
  "<ref>plain ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>''it'' ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>'''bo''' ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>[[L|t]] ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>[http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>https://example.org/a ; plain</ref>",
  "<ref>https://example.org/a ; https://example.org/a</ref>",
  "<ref>https://example.org/a ; RFC 2119</ref>",
  "<ref>https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>https://example.org/a ; * item</ref>",
  "<ref>RFC 2119 ; plain</ref>",
  "<ref>RFC 2119 ; https://example.org/a</ref>",
  "<ref>RFC 2119 ; RFC 2119</ref>",
  "<ref>RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>RFC 2119 ; * item</ref>",
  "<ref>CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>__NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>---- ; plain</ref>",
  "<ref>---- ; ''it''</ref>",
  "<ref>---- ; '''bo'''</ref>",
  "<ref>---- ; [[L|t]]</ref>",
  "<ref>---- ; [http://example.com x]</ref>",
  "<ref>---- ; https://example.org/a</ref>",
  "<ref>---- ; RFC 2119</ref>",
  "<ref>---- ; CO<sub>2</sub></ref>",
  "<ref>---- ; a<!--c-->b</ref>",
  "<ref>---- ; __NOTOC__</ref>",
  "<ref>---- ; ----</ref>",
  "<ref>---- ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>---- ; * item</ref>",
  "<ref>---- ; {{T|x=y}}</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; plain</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; ''it''</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; '''bo'''</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; [[L|t]]</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; https://example.org/a</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; RFC 2119</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub></ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; __NOTOC__</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; ----</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; * item</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}</ref>",
  "<ref>* item ; ''it''</ref>",
  "<ref>* item ; '''bo'''</ref>",
  "<ref>* item ; [[L|t]]</ref>",
  "<ref>* item ; [http://example.com x]</ref>",
  "<ref>* item ; https://example.org/a</ref>",
  "<ref>* item ; RFC 2119</ref>",
  "<ref>* item ; CO<sub>2</sub></ref>",
  "<ref>* item ; a<!--c-->b</ref>",
  "<ref>* item ; __NOTOC__</ref>",
  "<ref>* item ; ----</ref>",
  "<ref>* item ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>* item ; {{T|x=y}}</ref>",
  "<ref>{{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "{{T|v=plain ; RFC 2119}}",
  "{{T|v=plain ; __NOTOC__}}",
  "{{T|v=plain ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=''it'' ; CO<sub>2</sub>}}",
  "{{T|v=''it'' ; __NOTOC__}}",
  "{{T|v=''it'' ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v='''bo''' ; CO<sub>2</sub>}}",
  "{{T|v='''bo''' ; __NOTOC__}}",
  "{{T|v='''bo''' ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=[[L|t]] ; CO<sub>2</sub>}}",
  "{{T|v=[[L|t]] ; __NOTOC__}}",
  "{{T|v=[[L|t]] ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=[http://example.com x] ; CO<sub>2</sub>}}",
  "{{T|v=[http://example.com x] ; __NOTOC__}}",
  "{{T|v=[http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=https://example.org/a ; CO<sub>2</sub>}}",
  "{{T|v=https://example.org/a ; __NOTOC__}}",
  "{{T|v=https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=RFC 2119 ; plain}}",
  "{{T|v=RFC 2119 ; RFC 2119}}",
  "{{T|v=RFC 2119 ; CO<sub>2</sub>}}",
  "{{T|v=RFC 2119 ; a<!--c-->b}}",
  "{{T|v=RFC 2119 ; __NOTOC__}}",
  "{{T|v=RFC 2119 ; ----}}",
  "{{T|v=RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=RFC 2119 ; * item}}",
  "{{T|v=RFC 2119 ; {{T|x=y}}}}",
  "{{T|v=CO<sub>2</sub> ; ''it''}}",
  "{{T|v=CO<sub>2</sub> ; '''bo'''}}",
  "{{T|v=CO<sub>2</sub> ; [[L|t]]}}",
  "{{T|v=CO<sub>2</sub> ; [http://example.com x]}}",
  "{{T|v=CO<sub>2</sub> ; https://example.org/a}}",
  "{{T|v=CO<sub>2</sub> ; RFC 2119}}",
  "{{T|v=CO<sub>2</sub> ; __NOTOC__}}",
  "{{T|v=CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=a<!--c-->b ; RFC 2119}}",
  "{{T|v=a<!--c-->b ; __NOTOC__}}",
  "{{T|v=a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=__NOTOC__ ; plain}}",
  "{{T|v=__NOTOC__ ; ''it''}}",
  "{{T|v=__NOTOC__ ; '''bo'''}}",
  "{{T|v=__NOTOC__ ; [[L|t]]}}",
  "{{T|v=__NOTOC__ ; [http://example.com x]}}",
  "{{T|v=__NOTOC__ ; https://example.org/a}}",
  "{{T|v=__NOTOC__ ; RFC 2119}}",
  "{{T|v=__NOTOC__ ; CO<sub>2</sub>}}",
  "{{T|v=__NOTOC__ ; a<!--c-->b}}",
  "{{T|v=__NOTOC__ ; __NOTOC__}}",
  "{{T|v=__NOTOC__ ; ----}}",
  "{{T|v=__NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=__NOTOC__ ; * item}}",
  "{{T|v=__NOTOC__ ; {{T|x=y}}}}",
  "{{T|v=---- ; RFC 2119}}",
  "{{T|v=---- ; __NOTOC__}}",
  "{{T|v=---- ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; plain}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; ''it''}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; '''bo'''}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; [[L|t]]}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; https://example.org/a}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; RFC 2119}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub>}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; __NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; ----}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; * item}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}}}",
  "{{T|v=* item ; RFC 2119}}",
  "{{T|v=* item ; __NOTOC__}}",
  "{{T|v=* item ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{blockquote|<ref name=\"vra\">x [http://www.protectcivilrights.org/pdf/voting/AlabamaVRA.pdf ''Voting Rights in Alabama (1982–2006)''] {{Webarchive|url=x|date=y}} z</ref>}}",
  "{{T|v={{T|x=y}} ; RFC 2119}}",
  "{{T|v={{T|x=y}} ; __NOTOC__}}",
  "{{T|v={{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{#vardefine:x|1}}",
  "{{#vardefine:x|{{T|y}}}}",
  "{{#iferror:<strong class=\"error\">x</strong>|bad|ok}}",
  "{|\n| plain ; https://example.org/a\n|}",
  "{|\n| plain ; RFC 2119\n|}",
  "{|\n| plain ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| ''it'' ; CO<sub>2</sub>\n|}",
  "{|\n| ''it'' ; a<!--c-->b\n|}",
  "{|\n| ''it'' ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| ''it'' ; {{T|x=y}}\n|}",
  "{|\n| '''bo''' ; CO<sub>2</sub>\n|}",
  "{|\n| '''bo''' ; a<!--c-->b\n|}",
  "{|\n| '''bo''' ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| '''bo''' ; {{T|x=y}}\n|}",
  "{|\n| [[L|t]] ; CO<sub>2</sub>\n|}",
  "{|\n| [[L|t]] ; a<!--c-->b\n|}",
  "{|\n| [[L|t]] ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| [[L|t]] ; {{T|x=y}}\n|}",
  "{|\n| [http://example.com x] ; CO<sub>2</sub>\n|}",
  "{|\n| [http://example.com x] ; a<!--c-->b\n|}",
  "{|\n| [http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| [http://example.com x] ; {{T|x=y}}\n|}",
  "{|\n| https://example.org/a ; plain\n|}",
  "{|\n| https://example.org/a ; https://example.org/a\n|}",
  "{|\n| https://example.org/a ; RFC 2119\n|}",
  "{|\n| https://example.org/a ; CO<sub>2</sub>\n|}",
  "{|\n| https://example.org/a ; a<!--c-->b\n|}",
  "{|\n| https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| https://example.org/a ; * item\n|}",
  "{|\n| https://example.org/a ; {{T|x=y}}\n|}",
  "{|\n| RFC 2119 ; plain\n|}",
  "{|\n| RFC 2119 ; https://example.org/a\n|}",
  "{|\n| RFC 2119 ; RFC 2119\n|}",
  "{|\n| RFC 2119 ; CO<sub>2</sub>\n|}",
  "{|\n| RFC 2119 ; a<!--c-->b\n|}",
  "{|\n| RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| RFC 2119 ; * item\n|}",
  "{|\n| RFC 2119 ; {{T|x=y}}\n|}",
  "{|\n| CO<sub>2</sub> ; ''it''\n|}",
  "{|\n| CO<sub>2</sub> ; '''bo'''\n|}",
  "{|\n| CO<sub>2</sub> ; [[L|t]]\n|}",
  "{|\n| CO<sub>2</sub> ; [http://example.com x]\n|}",
  "{|\n| CO<sub>2</sub> ; https://example.org/a\n|}",
  "{|\n| CO<sub>2</sub> ; RFC 2119\n|}",
  "{|\n| CO<sub>2</sub> ; __NOTOC__\n|}",
  "{|\n| CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| a<!--c-->b ; ''it''\n|}",
  "{|\n| a<!--c-->b ; '''bo'''\n|}",
  "{|\n| a<!--c-->b ; [[L|t]]\n|}",
  "{|\n| a<!--c-->b ; [http://example.com x]\n|}",
  "{|\n| a<!--c-->b ; https://example.org/a\n|}",
  "{|\n| a<!--c-->b ; RFC 2119\n|}",
  "{|\n| a<!--c-->b ; __NOTOC__\n|}",
  "{|\n| a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| __NOTOC__ ; CO<sub>2</sub>\n|}",
  "{|\n| __NOTOC__ ; a<!--c-->b\n|}",
  "{|\n| __NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| __NOTOC__ ; {{T|x=y}}\n|}",
  "{|\n| ---- ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; plain\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; ''it''\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; '''bo'''\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; [[L|t]]\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; https://example.org/a\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; RFC 2119\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub>\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; __NOTOC__\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; ----\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; * item\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}\n|}",
  "{|\n| * item ; https://example.org/a\n|}",
  "{|\n| * item ; RFC 2119\n|}",
  "{|\n| * item ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| {{T|x=y}} ; ''it''\n|}",
  "{|\n| {{T|x=y}} ; '''bo'''\n|}",
  "{|\n| {{T|x=y}} ; [[L|t]]\n|}",
  "{|\n| {{T|x=y}} ; [http://example.com x]\n|}",
  "{|\n| {{T|x=y}} ; https://example.org/a\n|}",
  "{|\n| {{T|x=y}} ; RFC 2119\n|}",
  "{|\n| {{T|x=y}} ; __NOTOC__\n|}",
  "{|\n| {{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  '[[Image:Foo.jpg|caption [[Link]] text]]',
  '[[Image:Foo.jpg|upright|caption [[Link]] text]]',
  '<imagemap>\n\nFile:Foo.jpg|thumb|alt=A\npoly 1 1 2 2 [[L|{{T|x}}]]\npoly 3 3 4 4 [[L2]]\n\n</imagemap>',
  '<pre>{{T|x=[[L|t]]}}</pre>',
  '<hiero>{{T|x=[[L|t]]}}</hiero>',
  '<categorytree>Category:Physics</categorytree>',
  '<categorytree>[[Category:Physics|Physics]]</categorytree>',
  '<categorytree>{{T|x=[[L|t]]}}</categorytree>',
];

const ROOT = path.resolve(__dirname, '..', '..', '..');
const CONFIGS = ['enwiki', 'jawiki', 'llwiki'];

for (const configName of CONFIGS) {
  const configPath = path.join(ROOT, 'config', `${configName}.json`);
  process.env.WIKI_CONFIG = configPath;
  newProto.config = configPath;
  nativeProto.config = configPath;

  for (const test of tests) {
    const ok = runTests([test], { name: `pipeline-${configName}` });
    if (!ok) {
      process.exit(1);
    }
  }
}

